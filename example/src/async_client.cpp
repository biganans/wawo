
#include <wawo.h>
#include "services/ServiceShare.h"

class hello_handler :
	public wawo::net::channel_inbound_handler_abstract,
	public wawo::net::channel_activity_handler_abstract
{

private:
	wawo::net::socketaddr m_addrinfo;
	int m_max_concurrency;
	int m_curr_peer_count;

public:
	hello_handler(wawo::net::socketaddr const& addrinfo, int max_concurrency = 1)
		:m_addrinfo(addrinfo),
		m_max_concurrency(max_concurrency),
		m_curr_peer_count(0)
	{
	}

	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::packet> const& income ) {
		WAWO_INFO(">>> %u", income->len() ) ;

		WWSP<wawo::packet> outp = wawo::make_shared<wawo::packet>();
		outp->write( income->begin(), income->len() );
		ctx->write(outp);
	}

	void closed( WWRP<wawo::net::channel_handler_context> const& ctx ) {
		WAWO_INFO("socket closed");
	}

	void async_spawn() {
		WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(m_addrinfo.so_family, m_addrinfo.so_type, m_addrinfo.so_protocol);

		int rt = so->open();
		WAWO_ASSERT(rt == wawo::OK);

		so->pipeline()->add_last( WWRP<wawo::net::channel_handler_abstract>(this) );
		rt = so->async_connect(m_addrinfo.so_address );

		WAWO_ASSERT(rt == wawo::OK || rt == wawo::E_SOCKET_CONNECTING );
	}

	void connected( WWRP<wawo::net::channel_handler_context> const& ctx ) {
		services::HelloProcessor::SendHello(ctx);

		++m_curr_peer_count;
		if (m_curr_peer_count < m_max_concurrency) {
			async_spawn();
		}
	}

	void error(WWRP<wawo::net::channel_handler_context> const& ctx) {
		async_spawn();
		ctx->fire_error();
	}
};

int main( int argc, char** argv ) {

	wawo::app application ;
	wawo::net::address remote_addr("127.0.0.1", 22310);

	wawo::net::socketaddr raddr;
	raddr.so_family = wawo::net::F_AF_INET;

	if (argc == 2) {
		raddr.so_type = wawo::net::T_DGRAM;
		raddr.so_protocol = wawo::net::P_WCP;
	} else {
		raddr.so_type = wawo::net::T_STREAM;
		raddr.so_protocol = wawo::net::P_TCP;
	}
	raddr.so_address = remote_addr;

	services::InitSndBytes();
	{
		WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(raddr.so_family, raddr.so_type, raddr.so_protocol);

		int rt = so->open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

		WWRP<wawo::net::channel_handler_abstract> hello = wawo::make_ref<hello_handler>( raddr,20 );
		so->pipeline()->add_last(hello);

		rt = so->async_connect(raddr.so_address);
		WAWO_ASSERT(rt == wawo::OK);
	}
	application.run_for();
	services::DeinitSndBytes();

	
	WAWO_WARN("[main]socket server exit done ...");
	return wawo::OK;
}