#ifndef _WAWO_NET_NODE_ABSTRACT_HPP
#define _WAWO_NET_NODE_ABSTRACT_HPP

#include <wawo/core.h>
#include <wawo/net/core/Listener_Abstract.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::core;

	template <class _PeerT>
	class Node_Abstract:
		public Listener_Abstract<typename _PeerT::PeerEventT>
	{

	public:
		typedef _PeerT PeerT;
		typedef typename _PeerT::PeerEventT PeerEventT;
		typedef typename _PeerT::MessageT MessageT;

		typedef PeerProxy<_PeerT>							PeerProxyT;
		typedef Listener_Abstract<PeerEventT>				ListenerT;

		typedef Node_Abstract<PeerT> NodeAbstractT;
	private:
		WWRP<PeerProxyT> m_peer_proxy;

	public:
		Node_Abstract():
			m_peer_proxy( NULL )
		{
		}

		virtual ~Node_Abstract() {
			Stop();
			m_peer_proxy = NULL;
		}

		int StartListen(SocketAddr const& addr, wawo::net::SockBufferConfig const& cfg = GetBufferConfig(SBCT_MEDIUM) ) {
			WAWO_ASSERT( m_peer_proxy != NULL );
			return m_peer_proxy->StartListenOn(addr,cfg);
		}

		int StopListen( SocketAddr const& addr ) {
			WAWO_ASSERT( m_peer_proxy != NULL );
			return m_peer_proxy->StopListenOn(addr);
		}

		int Start() {
			m_peer_proxy = WWRP<PeerProxyT>(new PeerProxyT(WWRP<ListenerT>(this)));
			WAWO_CONDITION_CHECK( m_peer_proxy != NULL );

			int rt = m_peer_proxy->Start();
			WAWO_CONDITION_CHECK(rt == wawo::OK);
			return rt;
		}

		void Stop() {
			if( m_peer_proxy != NULL ) {
				m_peer_proxy->Stop();
			}
		}

		inline int WatchPeerSocket( WWRP<Socket> const& socket, int const& io_evt) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->WatchPeerSocket(socket,io_evt);
		}

		int UnWatchPeerSocket(WWRP<Socket> const& socket, int const& io_evt) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->UnWatchPeerSocket(socket, io_evt);
		}

		inline int AddPeer( WWRP<PeerT> const& peer ) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->AddPeer(peer);
		}

		inline int RemovePeer(WWRP<PeerT> const& peer) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->RemovePeer(peer);
		}
		inline int WatchPeerEvent(WWRP<PeerT> const& peer, u32_t const& evt_id) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->WatchPeerEvent(peer, evt_id);
		}

		inline int UnWatchPeerEvent(WWRP<PeerT> const& peer, u32_t const& evt_id) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->UnWatchPeerEvent(peer, evt_id);
		}

		int Connect( WWRP<PeerT> const& peer, WWRP<Socket> const& socket, bool const& nonblocking = false) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->Connect(peer,socket,nonblocking);
		}

		int Connect(WWRP<PeerT> const& peer, WWRP<Socket> const& socket, void const* const cookie, u32_t const& cookie_size, bool const& nonblocking = false) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->Connect(peer,socket,cookie,cookie_size,nonblocking);
		}

		void OnEvent( WWRP<PeerEventT> const& evt ) {

			WAWO_ASSERT(evt->GetPeer() != NULL );
			u32_t const& id = evt->GetId();

			switch( id ) {
			case wawo::net::PE_SOCKET_CONNECTED:
				{
					WAWO_ASSERT(evt->GetSocket() != NULL);
					OnPeerSocketConnected(evt);
				}
				break;
			case wawo::net::PE_CONNECTED:
				{
					OnPeerConnected(evt);
				}
				break;
			case wawo::net::PE_MESSAGE:
				{
					OnPeerMessage(evt);
				}
				break;
			case wawo::net::PE_SOCKET_RD_SHUTDOWN:
				{
					WAWO_ASSERT(evt->GetSocket() != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket rd shutdown, %p", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), evt->GetPeer().Get());
					OnPeerSocketReadShutdown(evt);
				}
				break;
			case wawo::net::PE_SOCKET_WR_SHUTDOWN:
				{
					WAWO_ASSERT(evt->GetSocket() != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket wr shutdown, %p", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), evt->GetPeer().Get());
					OnPeerSocketWriteShutdown(evt);
				}
				break;
			case wawo::net::PE_SOCKET_CLOSE:
				{
					WAWO_ASSERT(evt->GetSocket() != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket close, %p", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), evt->GetPeer().Get());
					OnPeerSocketClose(evt);
				}
				break;
			case wawo::net::PE_SOCKET_ERROR:
				{
					WAWO_ASSERT(evt->GetSocket() != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket error, %p", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), evt->GetPeer().Get() );
					OnPeerSocketError(evt);
				}
				break;
			case wawo::net::PE_CLOSE:
				{
					OnPeerClose(evt);
				}
				break;
			default:
				{
					WAWO_DEBUG("[node_abstract] custom peer evt: %d", id);
					OnCustomPeerEvent(evt);
				}
				break;
			}
		}

		virtual void OnPeerSocketConnected(WWRP<PeerEventT> const& evt) {
			WatchPeerEvent(evt->GetPeer(), PE_SOCKET_RD_SHUTDOWN);
			WatchPeerEvent(evt->GetPeer(), PE_SOCKET_WR_SHUTDOWN);
			WatchPeerEvent(evt->GetPeer(), PE_SOCKET_CLOSE);
			WatchPeerSocket(evt->GetSocket(), wawo::net::IOE_READ);
		}
		virtual void OnPeerSocketReadShutdown(WWRP<PeerEventT> const& evt) {
			WAWO_DEBUG("[node_abstract]OnPeerSocketReadShutdown: %d, ec: %d", evt->GetSocket()->GetFd(), evt->GetCookie().int32_v);
			evt->GetSocket()->Close(evt->GetCookie().int32_v);
		}
		virtual void OnPeerSocketWriteShutdown(WWRP<PeerEventT> const& evt) {
			WAWO_DEBUG("[node_abstract]OnPeerSocketWriteShutdown: %d, ec: %d", evt->GetSocket()->GetFd(), evt->GetCookie().int32_v);
			evt->GetSocket()->Close(evt->GetCookie().int32_v);
		}
		virtual void OnPeerSocketError(WWRP<PeerEventT> const& evt) {(void)evt;};
		virtual void OnPeerSocketClose(WWRP<PeerEventT> const& evt) {(void)evt;};

		virtual void OnPeerConnected(WWRP<PeerEventT> const& evt) {
			AddPeer(evt->GetPeer());
			WatchPeerEvent(evt->GetPeer(), PE_MESSAGE);
			WatchPeerEvent(evt->GetPeer(), PE_CLOSE);
		};
		virtual void OnPeerMessage(WWRP<PeerEventT> const& evt) = 0;
		virtual void OnPeerClose(WWRP<PeerEventT> const& evt) {(void)evt;};

		virtual void OnCustomPeerEvent(WWRP<PeerEventT> const& evt) {}
	};
}}
#endif