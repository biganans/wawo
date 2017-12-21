#include <wawo.h>

#include "services/EchoServer.hpp"

typedef wawo::net::peer::ros<> PeerT;
typedef wawo::net::ros_node<PeerT> NodeT;
typedef services::EchoServer<PeerT>	EchoProvider_Server;

using namespace wawo::net;


int main( int argc, char** argv ) {

	wawo::app application ;
	WWRP<NodeT> node = wawo::make_ref<NodeT>() ;

	int start_rt = node->start();
	if( start_rt != wawo::OK )
	{
		return start_rt;
	}

	WWRP<NodeT::SPT> echo_service_server = wawo::make_ref<EchoProvider_Server>(services::S_ECHO) ;
	node->add_service( services::S_ECHO, echo_service_server );

	wawo::net::socket_addr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::ST_STREAM;

	if (argc == 2) {
		laddr.so_type = wawo::net::ST_DGRAM;
		laddr.so_protocol = wawo::net::P_WCP;
	}
	else {
		laddr.so_type = wawo::net::ST_STREAM;
 		laddr.so_protocol = wawo::net::P_TCP;
	}

	laddr.so_address = wawo::net::address( "0.0.0.0",22310 );

	WAWO_LOG_WARN( "[main]start listen on : %s", laddr.so_address.address_info().cstr ) ;

//	int listen_rt = node->StartListen(addr);
	int listen_rt = node->start_listen(laddr);

	if( listen_rt != wawo::OK ) {
		WAWO_ERR("[main]start socket server failed: %d", start_rt );
		return listen_rt;
	}

	application.run_for();
	node->stop();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;
	return wawo::OK;

}
