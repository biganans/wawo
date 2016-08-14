#include <wawo.h>

#include "services/EchoClient.hpp"
#include "services/ServiceCommand.h"

typedef wawo::net::peer::Wawo<> PeerT;
typedef services::EchoClient<PeerT> EchoProvider_Client;

class NodeT :
	public wawo::net::WawoNode<PeerT>
{

public:
	void OnPeerClose(WWRP<PeerT::PeerEventT> const& evt) {
		WAWO_LOG_INFO("NodeT","peer closed");
	}
};

int main( int argc, char** argv ) {

	wawo::app::App application ;

	std::vector<wawo::net::AddrInfo> ips;
	WAWO_ENV_INSTANCE->GetLocalIpList(ips);

	WAWO_ASSERT( ips.size() > 0 );
	char server_ip[32] = {0};

	std::vector<wawo::net::AddrInfo>::iterator it = std::find_if( ips.begin(), ips.end(), [&](wawo::net::AddrInfo const& info) {
		if(
			wawo::strncmp(info.ip.CStr(), "10.",3) == 0 ||
			wawo::strncmp(info.ip.CStr(), "192.168.2.",10) == 0 ||
			wawo::strncmp(info.ip.CStr(), "192.168.1.",10) == 0
		) {
			return true;
		}
		return false;
	});

	WAWO_ASSERT( it != ips.end() );
	memcpy(server_ip,it->ip.CStr(), it->ip.Len());
	WAWO_ASSERT( wawo::strlen(server_ip) > 0 );

	WAWO_REF_PTR<NodeT> node( new NodeT() );

	int start_rt = node->Start();
	if( start_rt != wawo::OK )
	{
		return start_rt;
	}

	WAWO_REF_PTR<NodeT::SPT> echo_service( new EchoProvider_Client(services::S_ECHO) );
	node->AddService( services::S_ECHO, echo_service );

	wawo::net::SocketAddr remote_addr( "192.168.2.7", 12121 );

	int concurrency_client = 96;
	int i = 0;
	
	while( i++<concurrency_client ) {

		WAWO_REF_PTR<wawo::net::Socket> socket ( new wawo::net::Socket( remote_addr,wawo::net::F_AF_INET , wawo::net::ST_STREAM, wawo::net::P_TCP ) );
		WAWO_REF_PTR<PeerT> peer(new PeerT());

		int conn_rt = node->Connect(peer, socket);
		if( conn_rt != wawo::OK )
		{
			WAWO_LOG_FATAL("main", "connect socket failed, addr: %s, code: %d", socket->GetRemoteAddr().AddressInfo(), conn_rt );
			continue;
		}

		int tnonblocking = socket->TurnOnNonBlocking();
		if( tnonblocking != wawo::OK )
		{
			WAWO_LOG_FATAL("main", "turn on nonblocking failed, addr: %s, code: %d", socket->GetRemoteAddr().AddressInfo(), tnonblocking );
			continue;
		}

		peer->AttachSocket(socket);
		node->WatchPeerSocket(socket, wawo::net::IOE_READ);
		node->AddPeer(peer);

		node->WatchPeerEvent(peer,wawo::net::PE_CLOSE);
		node->WatchPeerEvent(peer,wawo::net::PE_MESSAGE);
		node->WatchPeerEvent(peer,wawo::net::PE_SOCKET_CLOSE);

		int rt;
		PEER_SND_HELLO(peer,rt);
	}

	application.RunUntil();
	node->Stop();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;

	return wawo::OK;
}