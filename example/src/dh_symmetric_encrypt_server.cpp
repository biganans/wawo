

#include <wawo.h>

class listen_handler :
	public wawo::net::socket_accept_handler_abstract
{
public:

	void accepted(WWRP<wawo::net::socket_handler_context> const& ctx, WWRP<wawo::net::socket> const& newso) {
		WWRP<wawo::net::socket_handler_abstract> hlen_handler = wawo::make_ref<wawo::net::handler::hlen>();
		newso->pipeline()->add_last(hlen_handler);

		WWRP<wawo::net::socket_handler_abstract> dh = wawo::make_ref<wawo::net::handler::dh_symmetric_encrypt>();
		newso->pipeline()->add_last(dh);

		WWRP<wawo::net::socket_handler_abstract> echo = wawo::make_ref<wawo::net::handler::echo>();
		newso->pipeline()->add_last(echo);
	}

};


int main(int argc, char** argv) {

	wawo::app app;

	WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(wawo::net::F_AF_INET, wawo::net::T_STREAM, wawo::net::P_TCP);
	int rt = so->open();

	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	wawo::net::address addr("0.0.0.0", 22311);
	rt = so->bind(addr);

	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	WWRP<wawo::net::socket_handler_abstract> h = wawo::make_ref<listen_handler>();
	so->pipeline()->add_last(h);

	rt = so->listen();

	if (rt != wawo::OK) {
		so->close();
		return rt;
	}

	app.run_for();
}