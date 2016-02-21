#include <wawo.h>

typedef wawo::net::WawoPeerProxy::MyPeerT MyPeerT;
typedef MyPeerT::MyCredentialT MyCredentialT;
typedef MyPeerT::MySocketT MySocketT;
typedef MyPeerT::MyMessageT MyMessageT;


typedef wawo::net::service::EchoClient<MyPeerT>				Echo_Client;

using namespace wawo::net::core;
using namespace wawo::net;

class ClientNode :
	public Listener_Abstract< typename WawoPeerProxy::MyPeerEventT >
{

public:
	typedef PeerProxy<MyPeerT>::MyPeerT MyPeerT;
	typedef PeerProxy<MyPeerT>::MySocketT MySocketT;
	typedef PeerProxy<MyPeerT>::MyCredentialT MyCredentialT;

	typedef PeerProxy<MyPeerT>::MyPeerEventT MyPeerEventT;
	typedef PeerProxy<MyPeerT>::MyServiceProviderT MyServiceProviderT;
	
	typedef Listener_Abstract< typename WawoPeerProxy::MyPeerEventT > MyListenerT;

private:
	WAWO_REF_PTR<WawoPeerProxy> m_proxy;

public:

	ClientNode():
		m_proxy( new WawoPeerProxy() )
	{
		WAWO_REF_PTR<MyServiceProviderT> echo_client( new Echo_Client(wawo::net::WSI_ECHO) );
		m_proxy->RegisterProvider( wawo::net::WSI_ECHO, echo_client );

		m_proxy->Register( PE_CONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Register( PE_DISCONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Register( PE_ERROR, WAWO_REF_PTR<MyListenerT>(this) );

		WAWO_CONDITION_CHECK( m_proxy != NULL );
		int rt = m_proxy->Start() ;
		WAWO_ASSERT( rt == wawo::OK );
	}

	virtual ~ClientNode() {
		WAWO_ASSERT( m_proxy != NULL );
		m_proxy->Stop();
	}

	void StopNode() {
		m_proxy->UnRegister( WAWO_REF_PTR<MyListenerT>(this) );
		m_proxy->Stop() ;
	}

	void AsyncConnect( WAWO_REF_PTR<MyPeerT> const& peer, WAWO_REF_PTR<MySocketT> const& socket ) {
		m_proxy->AsyncConnect(peer, socket);
	}

	void OnEvent( WAWO_REF_PTR<MyPeerEventT> const& evt ) {
		
		uint32_t id = evt->GetId();
		WAWO_REF_PTR<MyPeerT> peer = evt->GetCtx().peer;
		WAWO_REF_PTR<MySocketT> socket = evt->GetCtx().socket;

		switch( id ) {
		case wawo::net::PE_CONNECTED:
			{
				WAWO_LOG_INFO("main", "new peer connected, peer name: %s, remote socket addr: %s", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo() );
				peer->Echo_RequestHello();
			}
			break;
		case wawo::net::PE_DISCONNECTED:
			{
				WAWO_LOG_WARN("main", "peer disconnected, peer name: %s, remote socket addr: %s, disconnect code: %d", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo(), evt->GetEventData().int32_v );
			}
			break;
		case wawo::net::PE_ERROR:
			{
				WAWO_LOG_WARN("main", "peer error, peer name: %s, remote socket addr: %s, disconnect code: %d", peer->GetCredential().GetName().CStr(), socket->GetAddr().AddressInfo(), evt->GetEventData().int32_v );
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

	WAWO_REF_PTR<ClientNode> client_node( new ClientNode() );
	wawo::net::core::SocketAddr remote_addr( server_ip, 12121 );

	int concurrency_client = 96;
	int i = 0;
	while( i++<concurrency_client ) {
		MyPeerT::MyCredentialT credential( wawo::Len_CStr("mr_client_") + wawo::Len_CStr(std::to_string(i).c_str() ) );
		WAWO_REF_PTR<MyPeerT> peer( new MyPeerT(credential ) );
		WAWO_REF_PTR<MySocketT> socket ( new MySocketT( remote_addr, wawo::net::core::BufferConfigs[wawo::net::core::SBCT_MEDIUM] , Socket::TYPE_STREAM, Socket::PROTOCOL_TCP ) );
		client_node->AsyncConnect( peer, socket );
	}

	application.RunUntil();
	client_node->StopNode();

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;

	return wawo::OK;
}