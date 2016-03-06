#include <wawo.h>

typedef wawo::net::peer::Wawo<> PeerT;
typedef wawo::net::Node<PeerT> NodeT;
typedef wawo::net::service::EchoClient<PeerT> EchoProvider_Client;

int main( int argc, char** argv ) {

	wawo::app::App application ;

	std::vector<wawo::net::core::IpInfo> ips;
	WAWO_ENV_INSTANCE->GetLocalIpList(ips);

	WAWO_ASSERT( ips.size() > 0 );
	char server_ip[32] = {0};

	std::vector<wawo::net::core::IpInfo>::iterator it = std::find_if( ips.begin(), ips.end(), [&](wawo::net::core::IpInfo const& info) {
		if(
			wawo::strncmp(info.address, "10.",3) == 0 ||
			wawo::strncmp(info.address, "192.168.2.",10) == 0 ||
			wawo::strncmp(info.address, "192.168.1.",10) == 0
		) {
			return true;
		}
		return false;
	});

	WAWO_ASSERT( it != ips.end() );
	memcpy(server_ip,it->address, it->address_len);
	WAWO_ASSERT( wawo::strlen(server_ip) > 0 );

	WAWO_REF_PTR<NodeT> node( new NodeT() );

	int start_rt = node->Start();
	if( start_rt != wawo::OK )
	{
		return start_rt;
	}

	WAWO_REF_PTR<NodeT::MyServiceProviderT> echo_service( new EchoProvider_Client(wawo::net::WSI_ECHO) );
	node->AddServices( wawo::net::WSI_ECHO, echo_service );

	wawo::net::core::SocketAddr remote_addr( server_ip, 12121 );

	int concurrency_client = 96;
	int i = 0;
	while( i++<concurrency_client ) {

		WAWO_REF_PTR<PeerT::MySocketT> socket ( new PeerT::MySocketT( remote_addr, wawo::net::core::BufferConfigs[wawo::net::core::SBCT_MEDIUM] , wawo::net::core::Socket::TYPE_STREAM, wawo::net::core::Socket::PROTOCOL_TCP ) );

		int open_rt = socket->Open();
		if( open_rt != wawo::OK )
		{
			WAWO_LOG_FATAL("main", "open failed, addr: %s, code: %d", socket->GetAddr().AddressInfo(), open_rt );
			continue;
		}
		int conn_rt = socket->Connect();

		if( conn_rt != wawo::OK )
		{
			WAWO_LOG_FATAL("main", "connect to remote addr failed, addr: %s, code: %d", socket->GetAddr().AddressInfo(), open_rt );
			continue;
		}

		int turn_on_nonblocking = socket->TurnOnNonBlocking();
		if( conn_rt != wawo::OK )
		{
			WAWO_LOG_FATAL("main", "turn on nonblocking failed, addr: %s, code: %d", socket->GetAddr().AddressInfo(), open_rt );
			continue;
		}

		PeerT::MyCredentialT credential( wawo::Len_CStr("mr_client_") + wawo::Len_CStr(std::to_string(i).c_str() ) );
		WAWO_REF_PTR<PeerT> peer( new PeerT(credential ) );

		peer->AttachSocket(socket);
		node->AddPeer(peer);

		peer->Echo_RequestHello();
	}

	application.RunUntil();
	node->Stop();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;

	return wawo::OK;
}