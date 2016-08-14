#include <wawo.h>

#include "services/EchoServer.hpp"

typedef wawo::net::peer::Wawo<> PeerT;
typedef wawo::net::WawoNode<PeerT> NodeT;
typedef services::EchoServer<PeerT>	EchoProvider_Server;

using namespace wawo::net;

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
	memcpy(server_ip,it->ip.CStr(), it->ip.Len() );
	WAWO_ASSERT( wawo::strlen(server_ip) > 0 );

	WAWO_REF_PTR<NodeT> node( new NodeT() );

	int start_rt = node->Start();
	if( start_rt != wawo::OK )
	{
		return start_rt;
	}

	WAWO_REF_PTR<NodeT::SPT> echo_service_server( new EchoProvider_Server(services::S_ECHO) );
	node->AddService( services::S_ECHO, echo_service_server );

	wawo::net::SocketAddr addr( server_ip, 12121 );
	WAWO_LOG_WARN( "[main]start listen on : %s", addr.AddressInfo().CStr() ) ;

	int listen_rt = node->StartListen(addr);
	if( listen_rt != wawo::OK ) {
		WAWO_LOG_FATAL("[main]start socket server failed: %d", start_rt );
		return listen_rt;
	}

	application.RunUntil();
	node->Stop();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;
	return wawo::OK;
}
