#include <wawo.h>

//using namespace wawo;
//using namespace wawo::net;

class example_handler :
	public wawo::net::channel_inbound_handler_abstract,
	public wawo::net::channel_outbound_handler_abstract,
	public wawo::net::channel_activity_handler_abstract
{
public:
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::packet> const& income)
	{
		WAWO_INFO("<<<: %u bytes", income->len() );
		ctx->fire_read(income);
	}

	int write(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::packet> const& outlet)
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


class listen_server_handler:
	public wawo::net::channel_activity_handler_abstract,
	public wawo::net::channel_acceptor_handler_abstract
{
public:
	void accepted(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch )
	{
		WWRP<wawo::net::channel_handler_abstract> example = wawo::make_ref<example_handler>();
		ch->pipeline()->add_last(example);

		WWRP<wawo::net::channel_handler_abstract> echo = wawo::make_ref<wawo::net::handler::echo>();
		ch->pipeline()->add_last(echo);

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