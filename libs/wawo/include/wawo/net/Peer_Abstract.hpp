#ifndef _WAWO_NET_PEER_ABSTRACT_HPP
#define _WAWO_NET_PEER_ABSTRACT_HPP

#include <vector>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/net/Credential.hpp>
#include <wawo/thread/Mutex.h>

#include <wawo/net/core/NetEvent.hpp>
#include <wawo/net/core/Listener_Abstract.hpp>

#include <wawo/log/LoggerManager.h>

namespace wawo { namespace net {

	using namespace wawo::net::core;
	using namespace wawo::thread;

	template <class _MyPeerT>
	class PeerProxy_Abstract;

		enum PeerEventId {
			PE_CONNECTED = wawo::net::core::NE_MAX,
			PE_DISCONNECTED,
			PE_ERROR,
			PE_MESSAGE
		};


	template <class _MyPeerT>
	class PeerEvent :
		public wawo::net::core::Event
	{
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::MyMessageT MyMessageT;

	private:
		WAWO_REF_PTR<MyPeerT> m_peer;
		WAWO_SHARED_PTR<MyMessageT> m_incoming; //incoming message
		WAWO_SHARED_PTR<MyMessageT> m_related; //original request,for respond message
	public:
		explicit PeerEvent( uint32_t const& id, WAWO_REF_PTR<MyPeerT> const& peer ):
			Event(id),
			m_peer(peer),
			m_incoming(NULL),
			m_related(NULL)
		{}
		explicit PeerEvent( uint32_t const& id, WAWO_REF_PTR<MyPeerT> const& peer , int const& ec ):
			Event(id, EventData(ec)),
			m_peer(peer),
			m_incoming(NULL),
			m_related(NULL)
		{}
		explicit PeerEvent( uint32_t const& id, WAWO_REF_PTR<MyPeerT> const& peer , WAWO_SHARED_PTR<MyMessageT> const& incoming ):
			Event(id),
			m_peer(peer),
			m_incoming(incoming),
			m_related(NULL)
		{}
		explicit PeerEvent( uint32_t const& id, WAWO_REF_PTR<MyPeerT> const& peer , WAWO_SHARED_PTR<MyMessageT> const& incoming, WAWO_SHARED_PTR<MyMessageT> const& related ):
			Event(id),
			m_peer(peer),
			m_incoming(incoming),
			m_related(related)
		{}
		inline WAWO_REF_PTR<MyPeerT> const& GetPeer() const {
			return m_peer;
		}
		inline WAWO_SHARED_PTR<MyMessageT> const& GetIncoming() const {
			return m_incoming;
		}
		inline WAWO_SHARED_PTR<MyMessageT> const& GetRelated() const {
			return m_related;
		}
	};

	template <class _CredentialT, class _MessageT, class _SocketT>
	class Peer_Abstract :
		public Listener_Abstract< typename _SocketT::SocketEventT>
	{

	public:
		typedef _CredentialT MyCredentialT;
		typedef _MessageT MyMessageT;
		typedef _SocketT MySocketT;
		typedef typename _SocketT::SocketEventT MySocketEventT;

		typedef Listener_Abstract< typename _SocketT::SocketEventT> ListenerT;
		typedef Peer_Abstract<_CredentialT,_MessageT,_SocketT> MyBasePeerT;
		typedef PeerProxy_Abstract<MyBasePeerT> MyPeerProxyT;

	private:
		SharedMutex m_mutex;
		MyCredentialT m_credential;
		WAWO_REF_PTR<MyPeerProxyT> m_proxy;
		WAWO_REF_PTR<MySocketT> m_socket;
	public:
		explicit Peer_Abstract( MyCredentialT const& credential):
			m_credential(credential),
			m_proxy(NULL),
			m_socket(NULL)
		{
		}

		virtual ~Peer_Abstract() {
			/* 
			 * u must Detach your socket if the socket has been closed. 
			 */
			WAWO_ASSERT( m_socket == NULL );
		}

		MyCredentialT const& GetCredential() const {
			return m_credential;
		}

		virtual void Tick() {}

		void AssignProxy( WAWO_REF_PTR<MyPeerProxyT> const& proxy ) {
			LockGuard<SharedMutex> lg( m_mutex );
			m_proxy = proxy;
		}

		inline WAWO_REF_PTR<MyPeerProxyT> const& GetProxy() const { return m_proxy; }

		void OnEvent( WAWO_REF_PTR<MySocketEventT> const& evt) {

			WAWO_ASSERT( evt->GetSocket() == m_socket );
			int id = evt->GetId();

			switch( id ) {

			case SE_PACKET_ARRIVE:
				{
					int ec;
					WAWO_SHARED_PTR<MyMessageT> messages[WAWO_MAX_MESSAGE_COUNT_IN_ONE_PACKET];
					uint32_t count = MyMessageT::Decode( evt->GetPacket(), messages, WAWO_MAX_MESSAGE_COUNT_IN_ONE_PACKET, ec );

					if( ec != wawo::OK ) {
						WAWO_LOG_WARN( "client_wawo", "message parse error: %d", ec );
					}

					//parse message from packet, and trigger message
					for(uint32_t i=0;i<count;i++) {
						HandleIncomingMessage( messages[i] );
					}
				}
				break;
			case SE_SHUTDOWN:
			case SE_CLOSE:
				{
					HandleDisconnected( evt->GetEventData().int32_v );
				}
				break;
			default:
				{
					char tmp[256]={0};
					snprintf( tmp, sizeof(tmp)/sizeof(tmp[0]), "unknown socket evt: %d", id );
					WAWO_THROW_EXCEPTION( tmp );
				}
				break;
			}
		}

		//blocked until we received a new message arrived
		uint32_t ReceiveMessages( WAWO_SHARED_PTR<MyMessageT> messages[], uint32_t const& size, int& ec_o ) {
			WAWO_ASSERT( m_socket != NULL );

			uint32_t idx=0;

			WAWO_SHARED_PTR<Packet> packets[MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET];
			uint32_t count = m_socket->ReceivePackets( packets, MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET, ec_o );

			for( uint32_t pi=0;pi<count;pi++ ) {
				
				if( idx == size ) {
					WAWO_ASSERT( "please increase message container size" );
				}
				int ec;
				WAWO_SHARED_PTR<MyMessageT> messages_tmp[WAWO_MAX_MESSAGE_COUNT_IN_ONE_PACKET];
				uint32_t message_count = MyMessageT::Decode( packets[pi], messages_tmp, size, ec );
				WAWO_ASSERT( message_count != 0 );
				if( ec != wawo::OK ) {
					ec_o = ec;
					WAWO_LOG_FATAL("client_wawo", "decode message error: %d", ec );
					break;
				}

				for(uint32_t mi=0;mi<message_count;mi++) {
					if( idx == size ) { WAWO_THROW_EXCEPTION("please increase container size") }
					messages[idx++] = messages_tmp[mi];
				}
			}

			WAWO_LOG_DEBUG("client_wawo", "new messages arrived: %d", idx);
			return idx;
		}
		virtual int DoSend( WAWO_SHARED_PTR<MyMessageT> const& message ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);

			WAWO_ASSERT( message != NULL );
			WAWO_ASSERT( m_socket != NULL ) ;

			WAWO_SHARED_PTR<Packet> packet_o;
			int rt = MyMessageT::Encode( message, packet_o );
			if ( rt != wawo::OK ) {
				return rt;
			}

			return m_socket->SendPacket( packet_o );
		}

		void AttachSocket( WAWO_REF_PTR<MySocketT> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			WAWO_ASSERT( m_socket == NULL );

			WAWO_REF_PTR<ListenerT> peer_l( this );
			socket->Register(SE_PACKET_ARRIVE, peer_l );
			socket->Register(SE_SHUTDOWN, peer_l);
			socket->Register(SE_CLOSE, peer_l);

			m_socket = socket;
		}
		void DetachSocket() {
			LockGuard<SharedMutex> lg(m_mutex);
			if ( m_socket != NULL ) {
				WAWO_REF_PTR<ListenerT> peer_l( this );
				m_socket->UnRegister( peer_l );
				m_socket = NULL;
			}
		}

		inline WAWO_REF_PTR<MySocketT> const& GetSocket() const {
			return m_socket;
		}

		virtual void HandleDisconnected( int const& ec ) {
			m_proxy->HandleDisconnected( WAWO_REF_PTR<MyBasePeerT>( this ), ec );
		}
		virtual void HandleError( int const& ec ) {
			WAWO_ASSERT( m_proxy != NULL );
			m_proxy->HandleError( WAWO_REF_PTR<MyBasePeerT>( this ), ec );
		}

		virtual void HandleIncomingMessage( WAWO_SHARED_PTR<MyMessageT> const& message ) = 0;
	};
}}

#endif