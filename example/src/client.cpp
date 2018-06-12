
#include <wawo.h>
#include "services/ServiceShare.h"

class hello_handler :
	public wawo::net::channel_inbound_handler_abstract,
	public wawo::net::channel_activity_handler_abstract
{
private:
	wawo::net::address m_addr;
	int m_max_concurrency;
	int m_curr_peer_count;

public:
	hello_handler(wawo::net::address const& addr, int max_concurrency = 1)
		:m_addr(addr),
		m_max_concurrency(max_concurrency),
		m_curr_peer_count(0)
	{
	}

	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income ) {
		WAWO_INFO(">>> %u", income->len() ) ;

		WWRP<wawo::packet> outp = wawo::make_ref<wawo::packet>();
		outp->write( income->begin(), income->len() );
		ctx->write(outp);
	}

	void closed( WWRP<wawo::net::channel_handler_context> const& ctx ) {
		WAWO_INFO("socket closed");
	}

	/*
	void async_spawn() {
		WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(m_addrinfo.so_family, m_addrinfo.so_type, m_addrinfo.so_protocol);

		WWRP<wawo::net::channel_future> f = so->dial(m_addrinfo.so_address, [&](WWRP<wawo::net::channel> const& ch) {
			ch->pipeline()->add_last(WWRP<wawo::net::channel_handler_abstract>(this));
		});

		WAWO_ASSERT(f->get() == wawo::OK );
	}
	*/
	void connected( WWRP<wawo::net::channel_handler_context> const& ctx ) {
		services::HelloProcessor::SendHello(ctx);

		++m_curr_peer_count;
		if (m_curr_peer_count < m_max_concurrency) {
			//async_spawn();
		}
	}

	void error(WWRP<wawo::net::channel_handler_context> const& ctx) {
		//async_spawn();
		ctx->fire_error();
	}
};


int main( int argc, char** argv ) {

	wawo::app app ;
	wawo::net::address raddr("127.0.0.1", 22310);

	WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(wawo::net::F_AF_INET, wawo::net::T_STREAM, wawo::net::P_TCP);

	WWRP<wawo::net::channel_future> f = so->dial(raddr, [raddr](WWRP<wawo::net::channel> const& ch) {
		WWRP<wawo::net::channel_handler_abstract> hello = wawo::make_ref<hello_handler>(raddr, 2000);
		ch->pipeline()->add_last(hello);
	});

	WAWO_ASSERT(f->get() == wawo::OK);

	app.run_for();
	WAWO_WARN("[main]socket server exit done ...");
	

	return wawo::OK;
	
}