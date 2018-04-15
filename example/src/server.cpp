#include <wawo.h>

//using namespace wawo;
//using namespace wawo::net;

class example_handler :
	public wawo::net::channel_inbound_handler_abstract,
	public wawo::net::channel_outbound_handler_abstract,
	public wawo::net::channel_activity_handler_abstract
{
public:
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income)
	{
		WAWO_INFO("<<<: %u bytes", income->len() );
		ctx->fire_read(income);
	}

	int write(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& outlet)
	{
		WAWO_INFO(">>>: %u bytes", outlet->len() );
		return ctx->write(outlet);
	}
	
	void connected(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("connected: %d", ctx->ch->ch_id() );
		ctx->fire_connected();
	}

	void closed(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("closed: %d", ctx->ch->ch_id());
		ctx->fire_closed();
	}	

	void read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("read_shutdowned: %d", ctx->ch->ch_id() );
		ctx->close_write(wawo::net::SHUTDOWN_WR);
		ctx->fire_read_shutdowned();
	}
	void write_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("write_shutdowned: %d", ctx->ch->ch_id());
		ctx->fire_write_shutdowned();
	}

	void write_block(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("write_block: %d", ctx->ch->ch_id());
		ctx->fire_write_block();
	}

	void write_unblock(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("write_unblock: %d", ctx->ch->ch_id());
		ctx->fire_write_unblock();
	}
};


class http_server_handler:
	public wawo::ref_base
{
public:
	void on_header_end( WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::net::protocol::http::message> const& m )
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
		m->body = wawo::len_cstr( (char*)req->begin(), req->len() );

		WWRP<wawo::packet> outp;
		resp->encode(outp);

		ctx->write(outp);

		if(close_after_write) {
			ctx->close();
		}
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

	void accepted(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch )
	{
		//WWRP<wawo::net::channel_handler_abstract> example = wawo::make_ref<example_handler>();
		//ch->pipeline()->add_last(example);

		//WWRP<wawo::net::channel_handler_abstract> echo = wawo::make_ref<wawo::net::handler::echo>();
		//ch->pipeline()->add_last(echo);

		WWRP<wawo::net::handler::http> h = wawo::make_ref<wawo::net::handler::http>();
		h->bind<wawo::net::handler::fn_http_message_header_end_t > (wawo::net::handler::http_event::E_HEADER_COMPLETE, &http_server_handler::on_header_end, m_http_server, std::placeholders::_1, std::placeholders::_2);

		ch->pipeline()->add_last(h);

		(void) ctx;
	}
};


int main(int argc, char** argv) {
	wawo::app app;

	wawo::net::socketaddr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::T_STREAM;
	laddr.so_protocol = wawo::net::P_TCP;

	laddr.so_address = wawo::net::address("0.0.0.0", 22310);
	WWRP<wawo::net::socket> lsocket = wawo::make_ref<wawo::net::socket>(laddr.so_family, laddr.so_type, laddr.so_protocol);

	int open = lsocket->open();

	if (open != wawo::OK) {
		lsocket->close(open);
		return open;
	}

	int bind = lsocket->bind(laddr.so_address);
	if (bind != wawo::OK) {
		lsocket->close(bind);
		return bind;
	}

	WWRP<wawo::net::channel_handler_abstract> l_handler = wawo::make_ref<listen_server_handler>();
	lsocket->pipeline()->add_last(l_handler);

	int listen_rt = lsocket->listen();
	if (listen_rt != wawo::OK) {
		lsocket->close(listen_rt);
		return listen_rt;
	}

	app.run_for();
	return wawo::OK;
}