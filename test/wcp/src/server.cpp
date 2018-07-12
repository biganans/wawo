
#include <wawo.h>
#include "shared.hpp"

namespace wcp_test {

	struct async_send_broker :
		public wawo::ref_base
	{
		enum broker_state {
			S_SEND_BEGIN,
			S_SEND_CONTENT,
			S_SEND_END
		};

		wawo::spin_mutex m_mutex;

		wawo::u32_t s_blocked_at;
		wawo::u32_t s_total;
		wawo::byte_t* file_content;
		wawo::u32_t file_len;
		wawo::u32_t one_time_bytes;
		wawo::u32_t test_times;
		wawo::u32_t now_times;

		~async_send_broker(){
			WAWO_INFO("~async_send_broker");
		}

		bool blocked;
		int state;
		bool writing;
		void on_block(WWRP<wawo::net::channel_handler_context> const& ctx) {
			ctx->ch->end_read();
			WAWO_INFO("[wcp_test][%d]wr_block, end async read", ctx->ch->ch_id() );
		}

		void on_unblock(WWRP<wawo::net::channel_handler_context> const& ctx) {
			WAWO_INFO("[wcp_test][%d]wr_unblock, begin async read", ctx->ch->ch_id() );
			WAWO_ASSERT(blocked == true);
			blocked = false;

			state = S_SEND_BEGIN;
			begin_send(ctx);
			ctx->ch->begin_read();
		}

		void begin_send(WWRP<wawo::net::channel_handler_context> const& ctx) {
			wawo::lock_guard<wawo::spin_mutex> lg(m_mutex);

			if (state == S_SEND_BEGIN) {
				_begin_send_header(ctx);
				return;
			}

			if (state == S_SEND_CONTENT) {
				_begin_send_content(ctx);
				return;
			}

			WAWO_ASSERT(!"WHAT");
		}

		void _begin_send_header(WWRP<wawo::net::channel_handler_context> const& ctx) {

			WWRP<wawo::packet> outp_BEGIN = wawo::make_ref<wawo::packet>();
			outp_BEGIN->write<wawo::u8_t>(wcp_test::C_TRANSFER_FILE_HEADER);
			outp_BEGIN->write<wawo::u32_t>(file_len);
			WWRP<wawo::net::channel_future> f_write = ctx->write(outp_BEGIN);
			WAWO_ASSERT(state == S_SEND_BEGIN);
			//WAWO_INFO("HEADER BEGIN");
			f_write->add_listener([ B=WWRP<async_send_broker>(this),ctx](WWRP<wawo::net::channel_future> const& f) {
				//WAWO_INFO("HEADER DONE, %d", f->get() );
				if (f->get() == wawo::OK) {
					wawo::lock_guard<wawo::spin_mutex> lg(B->m_mutex);
					B->state = S_SEND_CONTENT;
					B->_begin_send_content(ctx);
				}
				else {
					wawo::lock_guard<wawo::spin_mutex> lg(B->m_mutex);
					B->state = S_SEND_BEGIN;
					B->_begin_send_header(ctx);
				}
			});
		}

		void _begin_send_content(WWRP<wawo::net::channel_handler_context> const& ctx) {
			WAWO_ASSERT(state == S_SEND_CONTENT);
			wawo::u32_t to_sent = 0;
			if ((file_len - s_total) <= one_time_bytes) {
				to_sent = (file_len - s_total);
			} else {
				to_sent = one_time_bytes;
			}

			WWRP<wawo::packet> outp_CONTENT = wawo::make_ref<wawo::packet>();
			outp_CONTENT->write(file_content + s_total, to_sent);
			//WAWO_INFO("CONTENT BEGIN");
			WAWO_ASSERT(writing == false);
			writing = true;
			WWRP<wawo::net::channel_future> f_write = ctx->write(outp_CONTENT);
			f_write->add_listener([B=WWRP<async_send_broker>(this),ctx, to_sent](WWRP < wawo::net::channel_future> const& f) {
				WAWO_ASSERT(B->writing == true);
				B->writing = false ;

				int rt = f->get();
				//WAWO_INFO("CONTENT DONE, %d", rt);
				if (rt == wawo::OK) {
					wawo::lock_guard<wawo::spin_mutex> lg(B->m_mutex);
					WAWO_ASSERT(B->state == S_SEND_CONTENT);
					B->s_total += to_sent;

					if (B->s_total == B->file_len) {
#if FAST_TRANSFER
						if (B->now_times < B->test_times) {
							++B->now_times;
							B->s_total = 0;
							B->state = S_SEND_BEGIN;
							//WAWO_INFO("SEND DONE, LAUNCH NEW");
								//WAWO_INFO("SEND DONE, LAUNCH NEW BEGIN");
							B->state = S_SEND_BEGIN;
							B->_begin_send_header(ctx);
						}
						else {
							WAWO_ASSERT(!"TEST FINISHED");
						}
#endif
					} else {
						WAWO_ASSERT(B->s_total < B->file_len);
						B->_begin_send_content(ctx);
					}

				} else if (rt == wawo::E_CHANNEL_WRITE_BLOCK) {
					//WE'LL GET A WRITE_BLOCK notification, then pause read
					//rewind s_total
					B->blocked = true;
				}
				else {
					WAWO_ASSERT(!"write failed");
				}
			});
		}
	};

	class StreamNode :
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract,
		public wawo::thread_run_object_abstract
	{
		wawo::byte_t* m_file_content;
		wawo::u32_t m_file_len;

	public:
		StreamNode() {

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

			(void)rbytes;
			(void)fclosert;
		}

		~StreamNode() {}

		wawo::spin_mutex msg_queue_mutex;

		struct income_queue {
			WWRP<wawo::net::channel_handler_context> ctx;
			WWRP<wawo::packet> income;
		};
		std::queue< income_queue > msg_queue;

		void run() {
			{
				wawo::lock_guard<wawo::spin_mutex> _lg(msg_queue_mutex);
				while (msg_queue.size()) {

					income_queue in = msg_queue.front();
					msg_queue.pop();

					WWRP <wawo::net::channel_handler_context > ctx = in.ctx;
					WWRP <wawo::packet> inpack = in.income;

					WAWO_ASSERT(inpack->len());

					wawo::u8_t cmd = inpack->read<wawo::u8_t>();
					switch (cmd) {
					case wcp_test::C_TRANSFER_FILE:
					{
						WWRP<async_send_broker> B = wawo::make_ref<async_send_broker>();
						B->s_total = 0;
						B->one_time_bytes = 1 * 1024 * 8 - 1; //cmd+content
						B->file_content = m_file_content;
						B->file_len = m_file_len;
						B->test_times = -1;
						B->state = async_send_broker::S_SEND_BEGIN;
						B->blocked = false;
						B->writing = false;

						ctx->ch->set_ctx(B);
						B->_begin_send_header(ctx);
					}
					break;
					}
				}
			}
			wawo::this_thread::usleep(200);
		}

		void connected(WWRP<wawo::net::channel_handler_context> const& ctx ) {
			(void)ctx;
		}

		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income) {
			wawo::lock_guard < wawo::spin_mutex > _lg(msg_queue_mutex);
			msg_queue.push({ ctx,income });
		}

		void write_block(WWRP<wawo::net::channel_handler_context> const& ctx) {
			WWRP<async_send_broker> sender_broker = ctx->ch->get_ctx<async_send_broker>();
			WAWO_ASSERT(sender_broker != NULL);
			sender_broker->on_block(ctx);
		}

		void write_unblock(WWRP < wawo::net::channel_handler_context > const& ctx) {
			WWRP<async_send_broker> sender_broker = ctx->ch->get_ctx<async_send_broker>();
			WAWO_ASSERT(sender_broker != NULL);
			sender_broker->on_unblock(ctx);
		}

		void on_start() {}
		void on_stop() {}
	};
}

int main(int argc, char** argv) {
	wawo::app _app;

	WWRP<wcp_test::StreamNode> streamnode = wawo::make_ref<wcp_test::StreamNode>();
	int rt = streamnode->start();
	WAWO_ASSERT(rt == wawo::OK);

	std::string url;
	if (argc == 2) {
		url = std::string("wcp://0.0.0.0:32310");
	}
	else {
		url = std::string("tcp://0.0.0.0:32310");
	}

	WWRP<wawo::net::channel_future> f_listen = wawo::net::socket::listen_on(url, [&streamnode](WWRP < wawo::net::channel> const& ch) {
		ch->pipeline()->add_last(streamnode);
	});

	f_listen->channel()->ch_close_future()->add_listener([](WWRP<wawo::net::channel_future> const& f) {
		WAWO_INFO("[#%d]channel closed", f->channel()->ch_id());
	});

	f_listen->channel()->ch_close_future()->wait();
	streamnode->stop();

	WAWO_INFO("[roger]server exiting...");
	return 0;
}
