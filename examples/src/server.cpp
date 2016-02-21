#include <wawo.h>

typedef wawo::net::WawoPeerProxy::MyPeerT MyPeerT;
typedef wawo::net::service::EchoServer<MyPeerT>				Echo_Server;

using namespace wawo::net::core;
using namespace wawo::net;

class ServerNode :
	public Listener_Abstract< typename WawoPeerProxy::MyPeerEventT >
{

private:
	WAWO_REF_PTR<WawoPeerProxy> m_proxy;

	typedef PeerProxy<MyPeerT>::MyPeerT MyPeerT;
	typedef PeerProxy<MyPeerT>::MySocketT MySocketT;
	typedef PeerProxy<MyPeerT>::MyCredentialT MyCredentialT;

	typedef PeerProxy<MyPeerT>::MyPeerEventT MyPeerEventT;
	typedef PeerProxy<MyPeerT>::MyServiceProviderT MyServiceProviderT;
	
	typedef Listener_Abstract< typename WawoPeerProxy::MyPeerEventT > MyListenerT;
public:

	ServerNode():
		m_proxy( new WawoPeerProxy() )
	{
		WAWO_REF_PTR<MyServiceProviderT> echo_server( new Echo_Server(wawo::net::WSI_ECHO) );
		m_proxy->RegisterProvider( wawo::net::WSI_ECHO, echo_server );

		m_proxy->Register( PE_CONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Register( PE_DISCONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Register( PE_ERROR, WAWO_REF_PTR<MyListenerT>(this) );

		WAWO_CONDITION_CHECK( m_proxy != NULL );
		int rt = m_proxy->Start() ;
		WAWO_ASSERT( rt == wawo::OK );
	}

	virtual ~ServerNode() {
		WAWO_ASSERT( m_proxy != NULL );
		m_proxy->Stop();
	}


	int StartListen( wawo::net::core::SocketAddr const& addr ) {
		return m_proxy->StartListenOn(addr);
	}

	void StopNode() {
		m_proxy->UnRegister( WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Stop() ;
	}

	void OnEvent( WAWO_REF_PTR<MyPeerEventT> const& evt ) {
		
		uint32_t id = evt->GetId();
		WAWO_REF_PTR<MyPeerT> peer = evt->GetCtx().peer;
		WAWO_REF_PTR<MySocketT> socket = evt->GetCtx().socket;

		switch( id ) {
		case wawo::net::PE_CONNECTED:
			{
				WAWO_LOG_INFO("main", "new peer connected, peer name: %s, remote socket addr: %s", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo() );
			}
			break;
		case wawo::net::PE_DISCONNECTED:
			{
				WAWO_LOG_INFO("main", "peer disconnected, peer name: %s, remote socket addr: %s, disconnect code: %d", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo(), evt->GetEventData().int32_v );
			}
			break;
		case wawo::net::PE_ERROR:
			{
				WAWO_LOG_INFO("main", "peer error, peer name: %s, remote socket addr: %s, disconnect code: %d", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo(), evt->GetEventData().int32_v );
			}
			break;
		}
	}
};

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


	WAWO_REF_PTR<ServerNode> server_node( new ServerNode() );
	wawo::net::core::SocketAddr addr( server_ip, 12121 );
	WAWO_LOG_WARN( "main", "start listen on : %s", addr.AddressInfo() ) ;

	int start_rt = server_node->StartListen(addr);
	if( start_rt != wawo::OK ) {
		WAWO_LOG_FATAL("main", "start socket server failed: %d", start_rt );
		return start_rt;
	}

	application.RunUntil();
	server_node->StopNode();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;

	return wawo::OK;
}