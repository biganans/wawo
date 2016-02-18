#ifndef _WAWO_NET_PEER_ABSTRACT_HPP
#define _WAWO_NET_PEER_ABSTRACT_HPP

#include <vector>

#include <wawo/core.h>
#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/Credential.hpp>
#include <wawo/SmartPtr.hpp>

#include <wawo/net/core/NetEvent.hpp>
#include <wawo/thread/Mutex.h>

#include <wawo/log/LoggerManager.h>

namespace wawo { namespace net {

	using namespace wawo::net::core;
	using namespace wawo::thread;

	template <class _MyPeerT>
	class PeerProxy_Abstract;

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

		struct PeerCtxInfo {
			WAWO_REF_PTR<MyBasePeerT>		peer;
			WAWO_REF_PTR<MySocketT>			socket;
			WAWO_SHARED_PTR<MyMessageT>		message;
		};

	private:
		SharedMutex m_mutex;
		MyCredentialT m_credential;
		WAWO_REF_PTR<MyPeerProxyT> m_proxy;
	public:
		explicit Peer_Abstract( MyCredentialT const& credential):
			m_credential(credential)
		{
		}

		virtual ~Peer_Abstract() {
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
						HandleIncomingMessage( evt->GetSocket(), messages[i] );
					}
				}
				break;
			case SE_SHUTDOWN:
			case SE_CLOSE:
				{
					HandleDisconnected( evt->GetSocket(), evt->GetEventData().int32_v );
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
		uint32_t ReceiveMessages( WAWO_REF_PTR<MySocketT> const& socket, WAWO_SHARED_PTR<MyMessageT> messages[], uint32_t const& size, int& ec_o ) {
			WAWO_ASSERT( socket != NULL );

			uint32_t idx=0;

			WAWO_SHARED_PTR<Packet> packets[MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET];
			uint32_t count = socket->ReceivePackets( packets, MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET, ec_o );

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
		virtual int DoSend( WAWO_REF_PTR<MySocketT> const& socket, WAWO_SHARED_PTR<MyMessageT> const& message ) {
			WAWO_ASSERT( message != NULL );
			WAWO_ASSERT( socket != NULL ) ;

			WAWO_SHARED_PTR<Packet> packet_o;
			int rt = MyMessageT::Encode( message, packet_o );
			if ( rt != wawo::OK ) {
				return rt;
			}

			return socket->SendPacket( packet_o );
		}

		virtual void AttachSocket( WAWO_REF_PTR<MySocketT> const& socket ) = 0;
		virtual void DetachSocket( WAWO_REF_PTR<MySocketT> const& socket ) = 0;
		virtual void GetAllSockets( std::vector< WAWO_REF_PTR<MySocketT> >& sockets ) = 0 ;

		virtual void HandleIncomingMessage( WAWO_REF_PTR<MySocketT> const& socket, WAWO_SHARED_PTR<MyMessageT> const& message ) = 0;
		virtual void HandleDisconnected( WAWO_REF_PTR<MySocketT> const& socket, int const& ec ) = 0;
		virtual void HandleError( WAWO_REF_PTR<MySocketT> const& socket, int const& ec ) = 0;
	};
}}
#endif