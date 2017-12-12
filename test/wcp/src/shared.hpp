#ifndef SHARED_HPP
#define SHARED_HPP

#include <wawo.h>

namespace wcp_test {
	enum FileTransferState {
		C_REQUEST,
		C_RECEIVE_HEADER,
		C_RECEIVE_CONTENT,
		C_RECEIVE_DONE
	};

	enum FileTransferCommand {
		C_TRANSFER_FILE ,
		C_TRANSFER_FILE_HEADER,
	};
}

#define TEST_PROTOCOL_WCP 1
#define TEST_FILE 99
#define FAST_TRANSFER 1

typedef wawo::net::peer::cargo<wawo::net::tlp::stream> StreamPeer;
wawo::net::socket_buffer_cfg sbc = { 512 * 1024, 512 * 1024 };

namespace wcp_test {

	using namespace wawo;
	using namespace wawo::net;

	struct async_send_broker:
		public wawo::net::listener_abstract< typename StreamPeer::peer_event_t>
	{
		typedef StreamPeer peer_t;
		typedef typename StreamPeer::peer_event_t EvtT;
		typedef wawo::net::listener_abstract< typename StreamPeer::peer_event_t> listener_t;

		enum broker_state {
			S_SEND_BEGIN,
			S_SEND_HEADER,
			S_SEND_CONTENT,
			S_SEND_END
		};

		spin_mutex m_mutex;

		WWRP<StreamPeer> peer;
		u32_t s_total;
		byte_t* file_content;
		u32_t file_len;
		u32_t one_time_bytes;
		i32_t test_times;
		i32_t now_times;

		int state;


		void on_event(WWRP<EvtT> const& evt) {
			
			switch (evt->id) {
			case E_WR_UNBLOCK:
				{
					WAWO_ASSERT(evt->info.int32_v == 0);
					WAWO_INFO("[wcp_test][#%d:%s]wr_unblock, begin async read", evt->so->get_fd(), evt->so->get_addr_info().cstr);
					begin_send();
					peer->get_socket()->begin_async_read();
				}
				break;
			case E_WR_BLOCK:
				{
					WAWO_INFO("[wcp_test][#%d:%s]wr_block, end async read", evt->so->get_fd(), evt->so->get_addr_info().cstr);
					peer->get_socket()->end_async_read();
				}
				break;
				default:
				{
					WAWO_THROW("invalid evt id");
				}
			}
		}

		void begin_send() {
			lock_guard<spin_mutex> lg(m_mutex);

			if (state == S_SEND_BEGIN) {
				state = S_SEND_HEADER;
			}

			if (state == S_SEND_HEADER) {
				_begin_send_header();
			}

			if (state == S_SEND_CONTENT) {
				_begin_send_content();
			}

			if (state == S_SEND_END) {
				peer->unregister_listener(E_WR_UNBLOCK, WWRP<listener_t>(this));
				peer->unregister_listener(E_WR_BLOCK, WWRP<listener_t>(this));

#if FAST_TRANSFER
				++ now_times;
				s_total = 0;
				state = S_SEND_BEGIN;

				if ((now_times < test_times) || test_times == -1) {

					peer->register_listener(E_WR_UNBLOCK, WWRP<listener_t>(this));
					peer->register_listener(E_WR_BLOCK, WWRP<listener_t>(this));

					WWRP<async_send_broker> sender_broker = WWRP<async_send_broker>(this);
					wawo::task::fn_lambda lambda = [sender_broker]() -> void {
						sender_broker->begin_send();
					};
					WAWO_SCHEDULER->schedule(lambda);
				}
#endif
			}
		}

		void _begin_send_header() {
			WWSP<packet> outp_BEGIN = wawo::make_shared<packet>();
			outp_BEGIN->write<u8_t>(wcp_test::C_TRANSFER_FILE_HEADER);
			outp_BEGIN->write<u32_t>(file_len);
			WWSP<peer_t::message_t> outm_BEGIN = wawo::make_shared<peer_t::message_t>(outp_BEGIN);
			int sndrt_BEGIN = peer->do_send_message(outm_BEGIN);
			if (sndrt_BEGIN == wawo::OK) { 
				state = S_SEND_CONTENT;
			}
		}

		void _begin_send_content() {
			do {
				wawo::u32_t to_sent = 0;
				if ((file_len - s_total) <= one_time_bytes) {
					to_sent = (file_len - s_total);
				}
				else {
					to_sent = one_time_bytes;
				}

				WWSP<packet> outp_CONTENT = wawo::make_shared<packet>();
				outp_CONTENT->write(file_content + s_total, to_sent);
				WWSP<peer_t::message_t> outm_CONTENT = wawo::make_shared<peer_t::message_t>(outp_CONTENT);

				int sndrt_CONTENT = peer->do_send_message(outm_CONTENT);
				if (sndrt_CONTENT == wawo::OK) { 
					s_total += to_sent;
				}
				else if (sndrt_CONTENT == wawo::E_SOCKET_SEND_BLOCK) {
					break;
				}
				else {
					break;
				}
			} while (s_total<file_len);

			if (s_total == file_len) {
				state = S_SEND_END;
			}
		}
	};

	class StreamNode :
		public wawo::thread::thread_run_object_abstract,
		public wawo::net::node_abstract<StreamPeer>
	{
		wawo::byte_t* m_file_content;
		wawo::u32_t m_file_len;

		typedef wawo::net::node_abstract<StreamPeer> NodeT;
	public:
		StreamNode() :
			node_abstract()
		{

#if TEST_FILE<8
			FILE* fp = fopen("../FILE_7", "rb");
#elif TEST_FILE<63
			FILE* fp = fopen("../FILE_63", "rb");
#else
			FILE* fp = fopen("../FILE", "rb");
#endif
			WAWO_ASSERT(fp != NULL);

			//long begin = ftell(fp);
			int seekrt = fseek(fp, 0L, SEEK_END);
			long end = ftell(fp);
			int seekbeg = fseek(fp, 0L, SEEK_SET);
			(void)seekrt;
			(void)seekbeg;

			m_file_len = end;
			m_file_content = new wawo::byte_t[m_file_len];
			::size_t rbytes = fread((char*)m_file_content, 1, m_file_len, fp);
			int fclosert = fclose(fp);

			(void) rbytes;
			(void) fclosert;
		}

		~StreamNode() {}

		spin_mutex evt_queue_mutex;
		std::queue< WWRP<peer_event_t> > evt_queue;

		virtual void run() {
			{
				lock_guard<spin_mutex> _lg(evt_queue_mutex);
				while (evt_queue.size()) {

					WWRP<peer_event_t> evt = evt_queue.front();
					evt_queue.pop();

					WWSP<packet> const& inpack = evt->message->data;
					WAWO_ASSERT(inpack->len());

					u8_t cmd = inpack->read<u8_t>();
					switch (cmd) {
					case wcp_test::C_TRANSFER_FILE:
					{
						WWRP<async_send_broker> sender_broker = wawo::make_ref<async_send_broker>();
						sender_broker->s_total = 0;
						sender_broker->one_time_bytes = 1 * 1024 * 8 - 1; //cmd+content
						sender_broker->file_content = m_file_content;
						sender_broker->file_len = m_file_len;
						sender_broker->peer = evt->peer;
						sender_broker->test_times = -1;
						sender_broker->state = async_send_broker::S_SEND_BEGIN;

						evt->peer->register_listener(E_WR_UNBLOCK, sender_broker);
						evt->peer->register_listener(E_WR_BLOCK, sender_broker);

						wawo::task::fn_lambda lambda = [sender_broker]() -> void {
							sender_broker->begin_send();
						};
						WAWO_SCHEDULER->schedule(lambda);

						//transfer_total += m_file_len;
						//++times;

						//if ((times % 200) == 0) {
						//	u64_t now = wawo::time::curr_milliseconds();
						//	printf( "speed: %llu\n", (transfer_total/ (now-begin_time)/1000) );
						//}

						//if (times > 1500) { break; }

						WAWO_INFO("run exit");
					}
					break;
					}

				}
			}
			wawo::this_thread::usleep(100);
		}

		void on_message(WWRP<peer_event_t> const& evt) {
			lock_guard<spin_mutex> _lg(evt_queue_mutex);
			evt_queue.push(evt);
		}

		void on_accepted(WWRP<StreamPeer> const& peer, WWRP<wawo::net::socket> const& so) {
		}

		void on_wr_block(WWRP<peer_event_t> const& evt) {
			WAWO_INFO("send block");
		}

		void on_wr_unblock(WWRP<peer_event_t> const& evt) {
			WAWO_INFO("send unblock");
		}

		void on_start() {}
		void on_stop() {}

		int start() {
			int startth = thread_run_object_abstract::start();

			if (startth != wawo::OK) {
				thread_run_object_abstract::stop();
				return startth;
			}

			int startnode = NodeT::start();
			if (startnode != wawo::OK) {
				thread_run_object_abstract::stop();
				return startnode;
			}

			return wawo::OK;
		}

		void stop() {
			NodeT::stop();
			thread_run_object_abstract::stop();
		}
	};

}



#endif
