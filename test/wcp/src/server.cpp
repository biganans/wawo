
#include <wawo.h>
#include "shared.hpp"



namespace wcp_test {

	struct async_send_broker :
		public wawo::ref_base
	{
		enum broker_state {
			S_SEND_BEGIN,
			S_SEND_HEADER,
			S_SEND_CONTENT,
			S_SEND_END
		};

		wawo::thread::spin_mutex m_mutex;

		wawo::u32_t s_total;
		wawo::byte_t* file_content;
		wawo::u32_t file_len;
		wawo::u32_t one_time_bytes;
		wawo::i32_t test_times;
		wawo::i32_t now_times;

		int state;

		void on_block(WWRP<wawo::net::socket_handler_context> const& ctx) {
			ctx->so->end_async_read();
			WAWO_INFO("[wcp_test][%s]wr_block, end async read", ctx->so->info().to_lencstr().cstr);
		}

		void on_unblock(WWRP<wawo::net::socket_handler_context> const& ctx) {
			WAWO_INFO("[wcp_test][%s]wr_unblock, begin async read", ctx->so->info().to_lencstr().cstr);
			begin_send(ctx);
			ctx->so->begin_async_read();
		}

		void begin_send(WWRP<wawo::net::socket_handler_context> const& ctx) {
			wawo::thread::lock_guard<wawo::thread::spin_mutex> lg(m_mutex);

			if (state == S_SEND_BEGIN) {
				state = S_SEND_HEADER;
			}

			while (state == S_SEND_HEADER) {
				_begin_send_header(ctx);
			}

			WAWO_ASSERT(state == S_SEND_CONTENT);
			_begin_send_content(ctx);

			if (state == S_SEND_END) {

#if FAST_TRANSFER
				++ now_times;
				s_total = 0;
				state = S_SEND_BEGIN;

				if ((now_times < test_times) || test_times == -1) {
					WWRP<async_send_broker> sender_broker = WWRP<async_send_broker>(this);
					wawo::task::fn_lambda lambda = [sender_broker, ctx]() -> void {
						sender_broker->begin_send(ctx);
					};
					WAWO_SCHEDULER->schedule(lambda);
				}
#endif
			}
		}

		void _begin_send_header(WWRP<wawo::net::socket_handler_context> const& ctx) {
			WWSP<wawo::packet> outp_BEGIN = wawo::make_shared<wawo::packet>();
			outp_BEGIN->write<wawo::u8_t>(wcp_test::C_TRANSFER_FILE_HEADER);
			outp_BEGIN->write<wawo::u32_t>(file_len);
			int sndrt_BEGIN = ctx->write(outp_BEGIN);
			
			if (sndrt_BEGIN == wawo::OK) {
				state = S_SEND_CONTENT;
			}
		}

		void _begin_send_content(WWRP<wawo::net::socket_handler_context> const& ctx) {
			do {
				wawo::u32_t to_sent = 0;
				if ((file_len - s_total) <= one_time_bytes) {
					to_sent = (file_len - s_total);
				}
				else {
					to_sent = one_time_bytes;
				}

				WWSP<wawo::packet> outp_CONTENT = wawo::make_shared<wawo::packet>();
				outp_CONTENT->write(file_content + s_total, to_sent);

				int sndrt_CONTENT = ctx->write(outp_CONTENT);
				if (sndrt_CONTENT == wawo::OK) {
					s_total += to_sent;
				}
				else if (sndrt_CONTENT == wawo::E_SOCKET_SEND_BLOCK) {
					break;
				}
				else {
					break;
				}
			} while (s_total < file_len);

			if (s_total == file_len) {
				state = S_SEND_END;
			}
		}
	};

	class StreamNode :
		public wawo::net::socket_inbound_handler_abstract,
		public wawo::net::socket_activity_handler_abstract,
		public wawo::thread::thread_run_object_abstract
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

		wawo::thread::spin_mutex msg_queue_mutex;

		struct income_queue {
			WWRP<wawo::net::socket_handler_context> ctx;
			WWSP<wawo::packet> income;
		};
		std::queue< income_queue > msg_queue;

		void run() {
			{
				wawo::thread::lock_guard<wawo::thread::spin_mutex> _lg(msg_queue_mutex);
				while (msg_queue.size()) {

					income_queue in = msg_queue.front();
					msg_queue.pop();

					WWRP < wawo::net::socket_handler_context > ctx = in.ctx;
					WWSP < wawo::packet > inpack = in.income;

					WAWO_ASSERT(inpack->len());

					wawo::u8_t cmd = inpack->read<wawo::u8_t>();
					switch (cmd) {
					case wcp_test::C_TRANSFER_FILE:
					{
						WWRP<async_send_broker> sender_broker = wawo::make_ref<async_send_broker>();
						sender_broker->s_total = 0;
						sender_broker->one_time_bytes = 1 * 1024 * 8 - 1; //cmd+content
						sender_broker->file_content = m_file_content;
						sender_broker->file_len = m_file_len;
						sender_broker->test_times = -1;
						sender_broker->state = async_send_broker::S_SEND_BEGIN;

						wawo::task::fn_lambda lambda = [sender_broker, ctx]() -> void {
							sender_broker->begin_send(ctx);
						};

						ctx->so->set_ctx(sender_broker);
						WAWO_SCHEDULER->schedule(lambda);
					}
					break;
					}
				}
			}
			wawo::this_thread::usleep(100);
		}

		void read(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& income) {
			wawo::thread::lock_guard < wawo::thread::spin_mutex > _lg(msg_queue_mutex);
			msg_queue.push({ ctx,income });
		}

		void write_block(WWRP<wawo::net::socket_handler_context> const& ctx) {
			WWRP<async_send_broker> sender_broker = ctx->so->get_ctx<async_send_broker>();
			WAWO_ASSERT(sender_broker != NULL);
			sender_broker->on_block(ctx);
		}

		void write_unblock(WWRP < wawo::net::socket_handler_context > const& ctx) {
			WWRP<async_send_broker> sender_broker = ctx->so->get_ctx<async_send_broker>();
			WAWO_ASSERT(sender_broker != NULL);
			sender_broker->on_unblock(ctx);
		}

		void on_start() {}
		void on_stop() {}
	};
}

WWRP<wcp_test::StreamNode> g_streamnode;

class server_listen_handler :
	public wawo::net::socket_accept_handler_abstract
{
public:
	void accepted(WWRP < wawo::net::socket_handler_context> const& ctx, WWRP<wawo::net::socket> const& newso) {
		//WWRP<wawo::net::socket_handler_abstract> h = wawo::make_ref<StreamNode>();
		newso->pipeline()->add_last(g_streamnode);

		(void)ctx;
	}
};


int main(int argc, char** argv) {

	wawo::app _app;

	g_streamnode = wawo::make_ref<wcp_test::StreamNode>();
	int rt = g_streamnode->start();
	WAWO_ASSERT(rt == wawo::OK);

	wawo::net::socketaddr laddr;
	laddr.so_address = wawo::net::address("0.0.0.0", 32310);
	laddr.so_family = wawo::net::F_AF_INET;

	if (argc == 2) {
		laddr.so_type = wawo::net::T_STREAM;
		laddr.so_protocol = wawo::net::P_TCP;
	}
	else {
		laddr.so_type = wawo::net::T_DGRAM;
		laddr.so_protocol = wawo::net::P_WCP;
	}

	WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(sbc, laddr.so_family, laddr.so_type, laddr.so_protocol);

	rt = so->open();
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	rt = so->bind(laddr.so_address);
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	WWRP<wawo::net::socket_handler_abstract> h = wawo::make_ref<server_listen_handler>();
	so->pipeline()->add_last(h);

	rt = so->listen();
	WAWO_ASSERT(rt == wawo::OK);

	_app.run_for();
	g_streamnode->stop();

	WAWO_INFO("[roger]server exiting...");
	return 0;
}
