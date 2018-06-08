
#include <wawo.h>

class send_hello :
	public wawo::net::channel_activity_handler_abstract
{
public:
	void connected(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_INFO("client connected");
		WWRP<wawo::packet> hello = wawo::make_ref<wawo::packet>();
		const char* hello_str = "hello there";
		hello->write((wawo::byte_t*)hello_str, wawo::strlen(hello_str) );
		ctx->write( hello );
	}
};

int main(int argc, char** argv) {

	wawo::app app;

	{
		WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(wawo::net::F_AF_INET, wawo::net::T_STREAM, wawo::net::P_TCP);
		int rt = so->open();

		if (rt != wawo::OK) {
			so->close();
			return rt;
		}

		WWRP<wawo::net::channel_handler_abstract> hlenh = wawo::make_ref<wawo::net::handler::hlen>();
		so->pipeline()->add_last(hlenh);

		WWRP<wawo::net::channel_handler_abstract> dhh = wawo::make_ref<wawo::net::handler::dh_symmetric_encrypt>();
		so->pipeline()->add_last(dhh);

		WWRP<wawo::net::channel_handler_abstract> dumpjn = wawo::make_ref<wawo::net::handler::dump_in_len>();
		so->pipeline()->add_last(dumpjn);

		WWRP<wawo::net::channel_handler_abstract> echoh = wawo::make_ref<wawo::net::handler::echo>();
		so->pipeline()->add_last(echoh);

		WWRP<wawo::net::channel_handler_abstract> sendh = wawo::make_ref<send_hello>();
		so->pipeline()->add_last(sendh);

		wawo::net::address addr("127.0.0.1", 22311);
		WWRP < wawo::net::channel_future> f_connect = so->async_connect(addr);

		WAWO_ASSERT(f_connect->get() == wawo::OK );
		so->ch_close_future()->wait();
	}

	app.run_for();
	return wawo::OK;
}