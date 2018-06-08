
#include <wawo.h>

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

		resp->body = wawo::len_cstr((char*)req->begin(), req->len());
		WWRP<wawo::packet> outp;
		resp->encode(outp);

		//WWRP<wawo::net::channel_promise> ch_promise = wawo::make_ref < wawo::net::channel_promise>();
		//ch_promise->add_listener([](WWRP<wawo::net::channel_future> const& f) {
			//WAWO_INFO("write rt: %d", f->get());
		//});
		//WWRP<wawo::net::channel_future> f_write = ctx->write(outp, ch_promise);
		WWRP<wawo::net::channel_future> f_write = ctx->write(outp);

		if (close_after_write) {
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


int main(int argc, char* argv) {

	wawo::app app;

	WWRP<http_server_handler> http_handler = wawo::make_ref<http_server_handler>();

	wawo::net::socketaddr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::T_STREAM;
	laddr.so_protocol = wawo::net::P_TCP;

	laddr.so_address = wawo::net::address("0.0.0.0", 8082);
	WWRP<wawo::net::socket> lsocket = wawo::make_ref<wawo::net::socket>(laddr.so_family, laddr.so_type, laddr.so_protocol);

	WWRP<wawo::net::channel_future> ch_listen_f = lsocket->listen_on(laddr.so_address, [http_handler](WWRP<wawo::net::channel> const& ch) {
		WWRP<wawo::net::handler::http> h = wawo::make_ref<my_http_handler>();
		h->bind<wawo::net::handler::fn_http_message_header_end_t >(wawo::net::handler::http_event::E_HEADER_COMPLETE, &http_server_handler::on_header_end, http_handler, std::placeholders::_1, std::placeholders::_2);
		ch->pipeline()->add_last(h);
	});

	int listen_rt = ch_listen_f->get();
	if (listen_rt != wawo::OK) {
		lsocket->ch_close();
		return listen_rt;
	}

	app.run_for();
	WWRP<wawo::net::channel_future> ch_close_f = lsocket->ch_close();
	WAWO_ASSERT(ch_close_f->get() == wawo::OK);
	lsocket->ch_close_future()->wait();

	WAWO_ASSERT(lsocket->ch_close_future()->is_done());
	WAWO_INFO("lsocket closed close: %d", lsocket->ch_close_future()->get());

	return wawo::OK;

}