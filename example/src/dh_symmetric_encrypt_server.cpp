

#include <wawo.h>

class listen_handler :
	public wawo::net::channel_acceptor_handler_abstract
{
public:

	void accepted(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch) {
		WWRP<wawo::net::channel_handler_abstract> hlen_handler = wawo::make_ref<wawo::net::handler::hlen>();
		ch->pipeline()->add_last(hlen_handler);

		WWRP<wawo::net::channel_handler_abstract> dh = wawo::make_ref<wawo::net::handler::dh_symmetric_encrypt>();
		ch->pipeline()->add_last(dh);

		WWRP<wawo::net::channel_handler_abstract> dumpjn = wawo::make_ref<wawo::net::handler::dump_in_len>();
		ch->pipeline()->add_last(dumpjn);

		WWRP<wawo::net::channel_handler_abstract> echo = wawo::make_ref<wawo::net::handler::echo>();
		ch->pipeline()->add_last(echo);
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
	WWRP<wawo::net::channel_future> f_bind = so->async_bind(addr);

	if (f_bind->get() != wawo::OK) {
		so->ch_close();
		return rt;
	}

	WWRP<wawo::net::channel_handler_abstract> h = wawo::make_ref<listen_handler>();
	so->pipeline()->add_last(h);

	WWRP<wawo::net::channel_future> f_listen = so->async_listen();

	if (f_listen->get() != wawo::OK) {
		so->ch_close();
		return rt;
	}

	app.run_for();
	so->ch_close_future()->wait();

	return wawo::OK;
}