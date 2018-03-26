
#include <wawo.h>


class ws_server_handler :
	public wawo::net::socket_activity_handler_abstract,
	public wawo::net::socket_accept_handler_abstract
{
public:
	void accepted(WWRP<wawo::net::socket_handler_context> const& ctx, WWRP<wawo::net::socket> const& newsocket) {
		WWRP<wawo::net::socket_handler_abstract> ws = wawo::make_ref<wawo::net::handler::websocket>();
		newsocket->pipeline()->add_last( ws );

		(void)ctx;
	}
};


int main(int argc, char** argv) {
	WAWO_INFO("main begin");

	wawo::app app;

	WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>( wawo::net::F_AF_INET, wawo::net::T_STREAM, wawo::net::P_TCP);

	int rt = so->open();
	
	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	wawo::net::address addr("0.0.0.0", 10080);
	rt = so->bind(addr);

	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	WWRP<wawo::net::socket_handler_abstract> lhandler = wawo::make_ref<ws_server_handler>();
	so->pipeline()->add_last( lhandler );

	rt = so->listen();

	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	app.run_for();

	WAWO_INFO("main end");
	return wawo::OK;
}