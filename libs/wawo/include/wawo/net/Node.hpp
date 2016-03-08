#ifndef _WAWO_NET_NODE_HPP
#define _WAWO_NET_NODE_HPP

#include <wawo/core.h>
#include <wawo/net/ServicePool.hpp>
#include <wawo/net/ServiceProvider_Abstract.hpp>
#include <wawo/net/Peer_Abstract.hpp>
#include <wawo/net/PeerProxy.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::core;

	template <class _MyPeerT>
	class Node:
		public Listener_Abstract< typename _MyPeerT::MyPeerEventT >
	{

	public:
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::MyPeerEventT MyPeerEventT;
		typedef typename _MyPeerT::MyMessageT MyMessageT;

		typedef ServiceProvider_Abstract<MyPeerT>				MyServiceProviderT;
		typedef typename MyServiceProviderT::ServicePeerT		MyServicePeerT;

		typedef typename MyServiceProviderT::PeerMessageCtx		MyPeerMessageCtxT;
		typedef ServicePool<MyServiceProviderT>					MyServicePoolT;
		typedef PeerProxy<MyPeerT>								MyPeerProxyT;
		typedef Listener_Abstract< MyPeerEventT >				MyListenerT;

	private:
		WAWO_REF_PTR< MyPeerProxyT > m_peer_proxy;
		WAWO_SHARED_PTR<MyServicePoolT> m_service_pool;

	public:
		Node():
			m_peer_proxy( new MyPeerProxyT() ),
			m_service_pool( new MyServicePoolT() )
		{
			m_peer_proxy->Register( PE_CONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
			m_peer_proxy->Register( PE_DISCONNECTED, WAWO_REF_PTR<MyListenerT>(this) );
			m_peer_proxy->Register( PE_ERROR, WAWO_REF_PTR<MyListenerT>(this) );
		}

		virtual ~Node()
		{
			m_peer_proxy = NULL;
			m_service_pool = NULL;
		}

		inline WAWO_REF_PTR<MyPeerProxyT> GetPeerProxy() const { return m_peer_proxy; }
		inline WAWO_SHARED_PTR<MyServicePoolT> GetServicePool() const { return m_service_pool; }

		void AddServices( uint32_t const& id, WAWO_REF_PTR<MyServiceProviderT> const& provider ) {
			WAWO_ASSERT( m_service_pool != NULL );
			m_service_pool->Register(id, provider );
		}

		void RemoveService(uint32_t const& id) {
			WAWO_ASSERT( m_service_pool != NULL );
			m_service_pool->UnRegister(id );
		}

		int StartListen(SocketAddr const& addr) {
			WAWO_ASSERT( m_peer_proxy != NULL );
			return m_peer_proxy->StartListenOn(addr);
		}

		int StopListen( SocketAddr const& addr ) {
			WAWO_ASSERT( m_peer_proxy != NULL );
			return m_peer_proxy->StopListenOn(addr);
		}

		int Start() {
			WAWO_CONDITION_CHECK( m_peer_proxy != NULL );
			int rt = m_peer_proxy->Start();
			WAWO_CONDITION_CHECK(rt == wawo::OK);

			return rt;
		}

		int Stop() {
			m_peer_proxy->UnRegister( WAWO_REF_PTR<MyListenerT>(this),true );
			m_peer_proxy->Stop();
			return wawo::OK;
		}

		int AddPeer( WAWO_REF_PTR<MyPeerT> const& peer ) {
			WAWO_ASSERT( peer->GetSocket() != NULL );
			WAWO_ASSERT( peer->GetSocket()->IsNonBlocking() );
			WAWO_ASSERT( m_peer_proxy != NULL );
			WAWO_ASSERT( peer != NULL );

			peer->Register( PE_MESSAGE, WAWO_REF_PTR<MyListenerT>(this) );
			m_peer_proxy->AddPeer(peer);

			return wawo::OK;
		}

		int AsyncConnect( WAWO_REF_PTR<MyPeerT> const& peer ) {
			WAWO_ASSERT( m_peer_proxy != NULL );
			WAWO_ASSERT( peer != NULL );

			return m_peer_proxy->AsyncConnect(peer, peer->GetSocket());
		}

		void OnEvent( WAWO_REF_PTR<MyPeerEventT> const& evt ) {

			uint32_t const& id = evt->GetId();
			WAWO_REF_PTR<MyPeerT> const& peer = evt->GetPeer();

			switch( id ) {
			case wawo::net::PE_CONNECTED:
				{
					peer->Register( PE_MESSAGE, WAWO_REF_PTR<MyListenerT>(this) );
					m_peer_proxy->AddPeer( peer );
				}
				break;
			case wawo::net::PE_DISCONNECTED:
				{
					peer->UnRegister( PE_MESSAGE, WAWO_REF_PTR<MyListenerT>(this) );
					m_peer_proxy->RemovePeer( peer );
				}
				break;
			case wawo::net::PE_ERROR:
				{
					WAWO_LOG_INFO("net_proxy", "[%s]peer error", peer->GetCredential().GetName().CStr() );
				}
				break;
			case wawo::net::PE_MESSAGE:
				{
					MyPeerMessageCtxT ctx;
					ctx.services = m_service_pool;
					ctx.peer = evt->GetPeer();
					ctx.message = evt->GetRelated();

					WAWO_SHARED_PTR<MyMessageT> const& incoming = evt->GetIncoming();
					WAWO_REF_PTR<MyServiceProviderT> provider = m_service_pool->Get(incoming->GetId());

					if( provider == NULL ) {
						WAWO_SHARED_PTR<Packet> packet( new Packet() );
						byte_t unknown[] = "service not available";
						packet->Write( (byte_t const* const)unknown, wawo::strlen((char const* const)(unknown)) );

						WAWO_SHARED_PTR<MyMessageT> message( new MyMessageT(incoming->GetId(), packet) );
						peer->Respond( message, incoming );
					} else {
						provider->WaitMessage( ctx, incoming );
					}
				}
				break;
			}
		}
	};
}}
#endif
