
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
		}
		else {
			resp->h.set("Connection", "close");
			close_after_write = true;
		}

		WWRP<wawo::packet> req;
		m->encode(req);
		m->body = wawo::len_cstr((char*)req->begin(), req->len());

		WWRP<wawo::packet> outp;
		resp->encode(outp);

		ctx->write(outp);

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
		ctx->close();
	}
};


class listen_server_handler :
	public wawo::net::channel_activity_handler_abstract,
	public wawo::net::channel_acceptor_handler_abstract
{
	WWRP<http_server_handler> m_http_server;
public:
	listen_server_handler()
	{
		m_http_server = wawo::make_ref<http_server_handler>();
	}

	~listen_server_handler() {}

	void accepted(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch)
	{
		WWRP<wawo::net::handler::http> h = wawo::make_ref<my_http_handler>();
		h->bind<wawo::net::handler::fn_http_message_header_end_t > (wawo::net::handler::http_event::E_HEADER_COMPLETE, &http_server_handler::on_header_end, m_http_server, std::placeholders::_1, std::placeholders::_2);
		ch->pipeline()->add_last(h);
	}
};

int main(int argc, char* argv) {

	wawo::app app;

	wawo::net::socketaddr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::T_STREAM;
	laddr.so_protocol = wawo::net::P_TCP;

	laddr.so_address = wawo::net::address("0.0.0.0", 8082);
	WWRP<wawo::net::socket> lsocket = wawo::make_ref<wawo::net::socket>(laddr.so_family, laddr.so_type, laddr.so_protocol);

	int open = lsocket->open();
	if (open != wawo::OK) {
		lsocket->close();
		return open;
	}

	WWRP<wawo::net::channel_future> ch_bind_f = lsocket->async_bind(laddr.so_address);
	int bindrt = ch_bind_f->get();
	if (bindrt != wawo::OK) {
		lsocket->close();
		return bindrt;
	}

	WWRP<wawo::net::channel_handler_abstract> l_handler = wawo::make_ref<listen_server_handler>();
	lsocket->pipeline()->add_last(l_handler);

	WWRP<wawo::net::channel_future> ch_listen_f = lsocket->async_listen();
	int listen_rt = ch_listen_f->get();
	if (listen_rt != wawo::OK) {
		lsocket->close();
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