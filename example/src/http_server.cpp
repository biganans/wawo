
#include <wawo.h>
//#include <vld.h>

class http_server_handler :
	public wawo::ref_base
{
public:
	void on_header_end(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::net::protocol::http::message> const& m)
	{
		//WAWO_INFO("[%d]", ctx->ch->ch_id());
		bool close_after_write = false;
		WWSP<wawo::net::protocol::http::message> resp = wawo::make_shared<wawo::net::protocol::http::message>();

		resp->status_code = 200;
		resp->status = "OK";

		resp->h.set("server", "wawo");

		if (m->h.have("Connection") && m->h.get("Connection") == "Keep-Alive") {
			resp->h.set("Connection", "Keep-Alive");
		} else {
			resp->h.set("Connection", "close");
			close_after_write = true;
		}

		WWRP<wawo::packet> req;
		m->encode(req);

		WWRP<wawo::packet> body = wawo::make_ref<wawo::packet>(req->len());
		body->write((wawo::byte_t*)req->begin(), req->len());
		resp->body = body;

		WWRP<wawo::packet> outp;
		resp->encode(outp);

		WWRP<wawo::net::channel_future> f_write = ctx->write(outp);
		//f_write->add_listener([](WWRP<wawo::net::channel_future> const& f) {
		//	WAWO_ASSERT(f->get() == wawo::OK);
			//WAWO_INFO("write rt: %d", f->get());
		//});
//		f_write->wait();

		if (close_after_write) {
			//WAWO_INFO("close fd: %d", ctx->ch->ch_id() );
			ctx->close();
		}
	}
};

class my_http_handler :
	public wawo::net::handler::http
{
public:
	~my_http_handler() {}
	void read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx) {
		ctx->fire_read_shutdowned();
		ctx->close();
	}
};

using namespace wawo;
using namespace wawo::net;

int main(int argc, char* argv) {
	int* vldtest = new int;

	//int fd[2];
	//int rt = wawo::net::socket_api::posix::socketpair(wawo::net::F_AF_INET, wawo::net::T_STREAM, wawo::net::P_TCP, fd);

	wawo::app app;
	WWRP<http_server_handler> http_handler = make_ref<http_server_handler>();

	WWRP<channel_future> ch_future = socket::listen_on("tcp://0.0.0.0:8082", [http_handler](WWRP<channel> const& ch) {
		WWRP<handler::http> h = make_ref<my_http_handler>();
		h->bind<handler::fn_http_message_header_end_t >(handler::http_event::E_HEADER_COMPLETE, &http_server_handler::on_header_end, http_handler, std::placeholders::_1, std::placeholders::_2);
		ch->pipeline()->add_last(h);
	});

	int listen_rt = ch_future->get();
	if (listen_rt != wawo::OK) {
		return listen_rt;
	}

	app.run();

	WWRP<channel_future> ch_close_f = ch_future->channel()->ch_close();
	WAWO_ASSERT(ch_close_f->get() == wawo::OK);
	ch_future->channel()->ch_close_future()->wait();

	WAWO_ASSERT(ch_future->channel()->ch_close_future()->is_done());
	WAWO_INFO("lsocket closed close: %d", ch_future->channel()->ch_close_future()->get());

	return wawo::OK;
}