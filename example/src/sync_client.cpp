#include <wawo.h>
#include "services/ServiceShare.h"


int main( int argc, char** argv ) {

	wawo::app application ;
	wawo::u64_t u64 = 0x0102030405060708;

	wawo::net::socket_addr remote_addr;
	remote_addr.so_family = wawo::net::F_AF_INET;
	remote_addr.so_type = wawo::net::ST_STREAM;
	remote_addr.so_protocol = wawo::net::P_TCP;
	remote_addr.so_address = wawo::net::address("192.168.2.42", 22310);

	
	do {

		//WWRP<wawo::net::Socket> socket = wawo::make_ref<wawo::net::Socket>( wawo::net::F_AF_INET, wawo::net::ST_STREAM, wawo::net::P_TCP);
		WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>( remote_addr.so_family,remote_addr.so_type, remote_addr.so_protocol );

		int rt = so->open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

		rt = so->connect(remote_addr.so_address);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

		WWRP<wawo::net::tlp_abstract> tlp = wawo::make_ref<typename services::peer_t::TLPT>();
		so->set_tlp(tlp);
		rt = so->tlp_handshake();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		WAWO_ASSERT( so->is_connected() );

//		socket->Close();
//		socket = NULL;

//		while (1) { wawo::this_thread::sleep(1); }
//		wawo::this_thread::sleep(100);
//		continue;

		WWRP<services::peer_t> peer = wawo::make_ref<services::peer_t>();
		peer->attach_socket(so);

		services::InitSndBytes();

		int sndhellort = services::HelloRequestCallBack::ReqHello(peer);

		int ec;
		do {

			WWSP<services::MessageT> arrives[5];
			wawo::u32_t count = peer->do_receive_messasges(arrives, 5, ec);

			WAWO_ASSERT(count == 0);
			WAWO_ASSERT(ec == wawo::OK);

		} while (ec == wawo::OK);

	} while (true);


	services::DeinitSndBytes();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;
	return wawo::OK;
}