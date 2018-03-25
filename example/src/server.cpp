#include <wawo.h>

//using namespace wawo;
//using namespace wawo::net;

class log_handler :
	public wawo::net::socket_inbound_handler_abstract,
	public wawo::net::socket_outbound_handler_abstract,
	public wawo::net::socket_activity_handler_abstract
{
public:
	void read(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& income)
	{
		WAWO_INFO("<<<: %u bytes", income->len() );
		ctx->fire_read(income);
	}

	void write(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& outlet)
	{
		WAWO_INFO(">>>: %u bytes", outlet->len() );
		ctx->write(outlet);
	}
	
	void connected(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("connected: %d", ctx->PIPE->SO->fd() );
		ctx->fire_connected();
	}

	void closed(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("closed: %d", ctx->PIPE->SO->fd());
		ctx->fire_closed();
	}	

	void read_shutdowned(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("read_shutdowned: %d", ctx->PIPE->SO->fd());
		ctx->PIPE->SO->shutdown(wawo::net::SHUTDOWN_WR);
		ctx->fire_read_shutdowned();
	}
	void write_shutdowned(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("write_shutdowned: %d", ctx->PIPE->SO->fd());
		ctx->fire_write_shutdowned();
	}

	void write_block(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("write_block: %d", ctx->PIPE->SO->fd());
		ctx->fire_write_block();
	}
	void write_unblock(WWRP<wawo::net::socket_handler_context> const& ctx) {
		WAWO_INFO("write_unblock: %d", ctx->PIPE->SO->fd());
		ctx->fire_write_unblock();
	}

};


class echo_handler :
	public wawo::net::socket_inbound_handler_abstract,
	public wawo::net::socket_activity_handler_abstract
{
public:
	void read(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& income)
	{
		ctx->write(income);
	}

	void connected(WWRP<wawo::net::socket_handler_context> const& ctx) {
		ctx->fire_connected();
	}

	void closed(WWRP<wawo::net::socket_handler_context> const& ctx, int const& code) {
		ctx->fire_closed();
	}
};

class listen_server_handler:
	public wawo::net::socket_activity_handler_abstract,
	public wawo::net::socket_accept_handler_abstract
{
public:
	void accepted(WWRP<wawo::net::socket_handler_context> const& ctx, WWRP<wawo::net::socket> const& newsocket)
	{
		WWRP<wawo::net::socket_handler_abstract> log = wawo::make_ref<log_handler>();
		newsocket->pipeline()->add_last(log);

		WWRP<echo_handler> echo = wawo::make_ref<echo_handler>();
		newsocket->pipeline()->add_last(echo);

		(void) ctx;
	}
};


int main(int argc, char** argv) {
	wawo::app app;

	wawo::net::socketaddr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::T_STREAM;

	if (argc == 2) {
		laddr.so_type = wawo::net::T_DGRAM;
		laddr.so_protocol = wawo::net::P_WCP;
	} else {
		laddr.so_type = wawo::net::T_STREAM;
		laddr.so_protocol = wawo::net::P_TCP;
	}

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

	int turn_on_nonblocking = lsocket->turnon_nonblocking();

	if (turn_on_nonblocking != wawo::OK) {
		lsocket->close(turn_on_nonblocking);
		return turn_on_nonblocking;
	}

	WWRP<wawo::net::socket_handler_abstract> l_handler = wawo::make_ref<listen_server_handler>();
	lsocket->pipeline()->add_last(l_handler);

	int listen_rt = lsocket->listen();
	if (listen_rt != wawo::OK) {
		lsocket->close(listen_rt);
		return listen_rt;
	}
	app.run_for();

}
