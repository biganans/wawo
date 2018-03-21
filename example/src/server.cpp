#include <wawo.h>

/*
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
*/


//class A {};
//class B : public A {};

//using namespace wawo;
//using namespace wawo::net;

static void cb_async_accept(WWRP<wawo::net::socket> const& newso, WWRP<wawo::ref_base> const& cookie_) {
	WAWO_ASSERT(cookie_ != NULL);

	WAWO_INFO("new connection accepted");

	//init pipeline
	
	return;
	/*
	WWRP<wawo::net::async_cookie> cookie = wawo::static_pointer_cast<wawo::net::async_cookie>(cookie_);
	WWRP<wawo::net::async_accept_cookie> user_cookie = wawo::static_pointer_cast<wawo::net::async_accept_cookie>(cookie->user_cookie);
	WAWO_ASSERT(user_cookie->node != NULL);

	WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
	WAWO_ASSERT(cookie->so != NULL);

	user_cookie->node->handle_async_accept(so);
	*/
}


int main(int argc, char** argv) {
	wawo::app app;

	wawo::net::socket_addr laddr;
	laddr.so_family = wawo::net::F_AF_INET;
	laddr.so_type = wawo::net::ST_STREAM;

	if (argc == 2) {
		laddr.so_type = wawo::net::ST_DGRAM;
		laddr.so_protocol = wawo::net::P_WCP;
	} else {
		laddr.so_type = wawo::net::ST_STREAM;
		laddr.so_protocol = wawo::net::P_TCP;
	}

	laddr.so_address = wawo::net::address("0.0.0.0", 22310);
	WWRP<wawo::net::socket> lsocket = wawo::make_ref<wawo::net::socket>(laddr.so_family, laddr.so_type, laddr.so_protocol);


	int open = lsocket->open();

	if (open != wawo::OK) {
		lsocket->close(open);
		return open;
	}

	if (laddr.so_protocol == wawo::net::P_WCP) {
		int reusert = lsocket->reuse_addr();
		if (reusert != wawo::OK) {
			lsocket->close(reusert);
			return reusert;
		}
		reusert = lsocket->reuse_port();
		if (reusert != wawo::OK) {
			lsocket->close(reusert);
			return reusert;
		}
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

	int listen_rt = lsocket->listen(WAWO_LISTEN_BACK_LOG);
	if (listen_rt != wawo::OK) {
		lsocket->close(listen_rt);
		return listen_rt;
	}

	WAWO_INFO("[peer_proxy]listen on: %s success, protocol: %s", lsocket->get_local_addr().address_info().cstr, wawo::net::protocol_str[laddr.so_protocol]);

	m_listen_sockets.insert(socket_pair(laddr.so_address, lsocket));

	WWRP<async_accept_cookie> cookie = wawo::make_ref<async_accept_cookie>();
	cookie->node = WWRP<node_t>(this);

	lsocket->begin_async_read(WATCH_OPTION_INFINITE, cookie, node_t::cb_async_accept, node_abstract::cb_async_accept_error);

	return wawo::OK;
	if (std::is_base_of<A, B>::value) {	
		printf("A->B ok");
	}
}
