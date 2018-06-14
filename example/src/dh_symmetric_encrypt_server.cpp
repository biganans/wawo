

#include <wawo.h>

int main(int argc, char** argv) {

	wawo::app app;

	WWRP<wawo::net::channel_future> f_listen = wawo::net::socket::listen_on("tcp://0.0.0.0:22311", [](WWRP<wawo::net::channel> const& ch) {
		WWRP<wawo::net::channel_handler_abstract> hlen_handler = wawo::make_ref<wawo::net::handler::hlen>();
		ch->pipeline()->add_last(hlen_handler);

		WWRP<wawo::net::channel_handler_abstract> dh = wawo::make_ref<wawo::net::handler::dh_symmetric_encrypt>();
		ch->pipeline()->add_last(dh);

		WWRP<wawo::net::channel_handler_abstract> dumpjn = wawo::make_ref<wawo::net::handler::dump_in_len>();
		ch->pipeline()->add_last(dumpjn);

		WWRP<wawo::net::channel_handler_abstract> echo = wawo::make_ref<wawo::net::handler::echo>();
		ch->pipeline()->add_last(echo);
	});

	int rt = f_listen->get();
	if (rt != wawo::OK) {
		return rt;
	}

	app.run();
	f_listen->channel()->ch_close();
	f_listen->channel()->ch_close_future()->wait();

	return wawo::OK;
}