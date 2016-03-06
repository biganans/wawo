#include <wawo.h>

typedef wawo::net::peer::Wawo<> PeerT;
typedef wawo::net::Node<PeerT> NodeT;

typedef wawo::net::service::EchoServer<PeerT>	EchoProvider_Server;

using namespace wawo::net::core;
using namespace wawo::net;


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

	WAWO_REF_PTR<NodeT::MyServiceProviderT> echo_service_server( new EchoProvider_Server(wawo::net::WSI_ECHO) );
	node->AddServices( wawo::net::WSI_ECHO, echo_service_server );


	wawo::net::core::SocketAddr addr( server_ip, 12121 );
	WAWO_LOG_WARN( "main", "start listen on : %s", addr.AddressInfo() ) ;

	int listen_rt = node->StartListen(addr);
	if( listen_rt != wawo::OK ) {
		WAWO_LOG_FATAL("main", "start socket server failed: %d", start_rt );
		return listen_rt;
	}

	application.RunUntil();
	node->Stop();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;

	return wawo::OK;
}