#include <wawo.h>
#include <vld.h>

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

	void write(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& outlet, WWRP<wawo::net::channel_promise> const& ch_promise)
	{
		WAWO_INFO(">>>: %u bytes", outlet->len() );
		ctx->write(outlet,ch_promise);
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
		WWRP<wawo::net::channel_promise> ch_promise = wawo::make_ref<wawo::net::channel_promise>();
		ctx->shutdown_write();
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


class my_echo :
	public wawo::net::handler::echo,
	public wawo::net::channel_activity_handler_abstract
{
public:
	~my_echo() {}
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income) {
		 WWRP<wawo::net::channel_future> ch_future = ctx->write(income);

		ch_future->add_listener([]( WWRP<wawo::net::channel_future> const& ch_future) {
			if (ch_future->is_success()) {
				int wrt = ch_future->get();
				WAWO_INFO("write rt: %d", wrt);
			}
		});
	}

	void read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx) {
		ctx->close();
	}
};

class listen_server_handler :
	public wawo::net::channel_activity_handler_abstract,
	public wawo::net::channel_acceptor_handler_abstract
{
public:
	listen_server_handler()
	{
	}

	~listen_server_handler() {}

	void accepted(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch )
	{
		WWRP<wawo::net::channel_handler_abstract> example = wawo::make_ref<example_handler>();
		ch->pipeline()->add_last(example);

		WWRP<wawo::net::channel_handler_abstract> echo = wawo::make_ref<my_echo>();
		ch->pipeline()->add_last(echo);
		(void) ctx;
	}
};

int main(int argc, char** argv) {

	int* p = new int(3);

	wawo::app app;

	wawo::net::socketaddr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::T_STREAM;
	laddr.so_protocol = wawo::net::P_TCP;

	laddr.so_address = wawo::net::address("0.0.0.0", 22310);
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