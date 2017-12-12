
#include <wawo.h>
#include "services/EchoClient.hpp"
#include "services/ServiceShare.h"

typedef wawo::net::peer::ros<> PeerT;
typedef services::EchoClient<PeerT> EchoProvider_Client;

class NodeT :
	public wawo::net::ros_node<PeerT>
{

public:
	void on_peer_close(WWRP<PeerT::peer_event_t> const& evt) {
		WAWO_LOG_INFO("NodeT","peer closed");
	}
};

int main( int argc, char** argv ) {

	wawo::app application ;

	WAWO_REF_PTR<NodeT> node = wawo::make_ref< NodeT>() ;
	int start_rt = node->start();
	if( start_rt != wawo::OK )
	{
		return start_rt;
	}

	WAWO_REF_PTR<NodeT::SPT> echo_service = wawo::make_ref<EchoProvider_Client>(services::S_ECHO) ;
	node->add_service( services::S_ECHO, echo_service );

	wawo::net::address remote_addr( "192.168.2.42", 22310);


	do {
		int concurrency_client = 5;
		int i = 0;

		services::InitSndBytes();
		wawo::u64_t now = wawo::time::curr_seconds();



		while (i < concurrency_client) {

			WAWO_REF_PTR<PeerT> peer = wawo::make_ref<PeerT>();
			wawo::net::socket_addr addr_info;

			addr_info.so_family = wawo::net::F_AF_INET;

			if (argc == 2) {
				addr_info.so_type = wawo::net::ST_DGRAM;
				addr_info.so_protocol = wawo::net::P_WCP;
			}
			else {
				addr_info.so_type = wawo::net::ST_STREAM;
				addr_info.so_protocol = wawo::net::P_TCP;
			}

			addr_info.so_address = remote_addr;

			int conn_rt = node->connect(peer, addr_info);
			if (conn_rt != wawo::OK)
			{
				//WAWO_ERR("main", "connect socket failed, addr: %s:%d, code: %d", addr_info.ip,addr_info.port , conn_rt );
				continue;
			}

			WAWO_ASSERT(peer->get_socket() != NULL);
			int tnonblocking = peer->get_socket()->turnon_nonblocking();
			if (tnonblocking != wawo::OK)
			{
				peer->close();
				peer->detach_socket();
				//WAWO_ERR("main", "turn on nonblocking failed, addr: %s:%d, code: %d", addr_info.ip, addr_info.port, tnonblocking);
				continue;
			}


			if (wawo::random_u8() < 100) {
				continue;
			}

			++i;

			node->watch_peer_socket(peer->get_socket(), wawo::net::IOE_READ);
			node->add_peer(peer);

			node->watch_peer_event(peer, wawo::net::PE_CLOSE);
			node->watch_peer_event(peer, wawo::net::PE_MESSAGE);

			int rt = services::HelloRequestCallBack::ReqHello(peer);
		}

		application.run_for(30);
		services::DeinitSndBytes();
		node->stop();

		WAWO_LOG_WARN("main", "socket server exiting ...");
		application.run_for(60 * 2 * 1000 + 2);

	} while (1);
	
	WAWO_LOG_WARN("main", "socket server exit done ...");
	return wawo::OK;
}