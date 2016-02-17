#ifndef _WAWO_NET_PEER_WAWO_HPP_
#define _WAWO_NET_PEER_WAWO_HPP_

//#include <wawo/net/ServiceList.h>

#include <wawo/net/core/Event.hpp>
#include <wawo/net/Credential.hpp>

#include <wawo/net/protocol/Wawo.hpp>
#include <wawo/net/message/Wawo.hpp>

#include <wawo/thread/Mutex.h>

#include <wawo/net/Peer_Abstract.hpp>
#include <wawo/net/Context.hpp>

#include <wawo/net/ServiceList.h>

namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;
	using namespace wawo::net;
	using namespace wawo::net::core;

	/*
	 * Notice (u'd better read this, if you have any question about wawo::net::client )
	 *
	 * a client identify one user who connected or want to connect to a remote server.
	 * it's API is simple as below

	 * 1, connect to remote server
	 * 2, disconnect from remote server
	 * 3, send message to remote server
	 * 4, hear message from remote server
	 * 5, identify itself on remote server, by credential
	 *
	 *
	 * wawo's impl of a client only have one connection, if you like, you can impl some kind of multi-connection client
	 *
	 * wawo's client impl do not have a status of login,this is left for yourself
	 * there are several ways to impl a login logic, if you have no clue where to start, pls read the lines below:

	 * First solution
	 * 1, extends wawo::net::client::wawo, add a state of login|logout, impl your own login message, switch to login state when you get right login response success
	   2, check this state for every message out and in

	 * Seconds solution
	 * 1, add kinds authorize message, once one authorize for some kinds of access is granted, add it to your permisson available table
	 * 2, check permission scope for each message out and in
	 */

	template <class _CredentialT = Credential, class _MessageT = wawo::net::message::Wawo, class _SocketT = wawo::net::core::BufferSocket<wawo::net::protocol::Wawo> >
	class Wawo :
		public wawo::net::Peer_Abstract<_CredentialT,_MessageT,_SocketT>
	{

	public:
		typedef wawo::net::Peer_Abstract<_CredentialT,_MessageT,_SocketT> MyBasePeerT;
		typedef typename MyBasePeerT::PeerCtxInfo MyPeerCtxInfoT ;

		typedef wawo::net::peer::Wawo<_CredentialT,_MessageT,_SocketT> MyPeerT;
		typedef wawo::net::PeerProxy_Abstract< MyBasePeerT> MyPeerProxyT;

		typedef _CredentialT MyCredentialT;
		typedef _MessageT MyMessageT;
		typedef _SocketT MySocketT;
		typedef typename _SocketT::SocketEventT MySocketEventT;

	private:
		struct RequestedMessage
		{
			WAWO_REF_PTR<MySocketT> socket;
			WAWO_SHARED_PTR<MyMessageT> message;
		};

		typedef std::vector< RequestedMessage > RequestedMessagePool ;

		SharedMutex m_socket_mutex;
		WAWO_REF_PTR<MySocketT> m_socket;

		SpinMutex m_requested_mutex;
		RequestedMessagePool m_requested ;

	public:
		explicit Wawo( MyCredentialT const& credential ):
			MyBasePeerT(credential)
		{
		}

		virtual ~Wawo() {
			LockGuard<SpinMutex> _lg( m_requested_mutex );
			m_requested.clear();
		}

		virtual void AttachSocket( WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( socket != NULL );

			LockGuard<SharedMutex> lg(m_socket_mutex);
			WAWO_ASSERT( m_socket == NULL );
			m_socket = socket;
			WAWO_ASSERT( socket->IsConnected() );
		}

		//unset connetion would result in S_IDLE,,,
		virtual void DetachSocket( WAWO_REF_PTR<MySocketT> const& socket ) {
			//multi-shutdown evt may be triggered sometimes, then we get multi-times DetachSocket
			//for example
			//send shutdown, recv get epip,,
			LockGuard<SharedMutex> lg(m_socket_mutex);
			if( m_socket != NULL ) {
				WAWO_ASSERT( m_socket == socket );

				LockGuard<SpinMutex> lg(m_requested_mutex);
				typename RequestedMessagePool::iterator it = m_requested.begin();
				while( it != m_requested.end() ) {
					if( it->socket == m_socket ) {
						WAWO_REF_PTR<Event> evt ( new Event( wawo::net::message::Wawo::E_ERROR, NULL ));
						it->message->TriggerEvent( evt );
						it = m_requested.erase(it);
					} else {
						++it;
					}
				}

				m_socket = NULL;
			}
		}

		virtual void GetAllSockets( std::vector< WAWO_REF_PTR<MySocketT> >& sockets ) {
			LockGuard<SharedMutex> lg(m_socket_mutex);
			if( m_socket != NULL ) {
				sockets.push_back(m_socket);
			}
		}

		virtual void Tick() {
			_CheckHeartBeat();
		}

		void _CheckHeartBeat() {
		 //@to impl
		}

		int Send( WAWO_SHARED_PTR<MyMessageT> const& message ) {
			SharedLockGuard<SharedMutex> lg(m_socket_mutex);

			if( m_socket == NULL ) {
				return wawo::E_CLIENT_NO_SOCKET_ATTACHED ;
			}

			WAWO_ASSERT( message != NULL);
			WAWO_ASSERT( message->GetType() == wawo::net::message::Wawo::T_NONE );

			message->SetType( wawo::net::message::Wawo::T_SEND );
			return MyBasePeerT::DoSend( m_socket, message );
		}

		int Request( WAWO_SHARED_PTR<MyMessageT> const& message ) {
			SharedLockGuard<SharedMutex> lg(m_socket_mutex);

			if( m_socket == NULL ) {
				return wawo::E_CLIENT_NO_SOCKET_ATTACHED ;
			}

			WAWO_ASSERT( message != NULL );
			WAWO_ASSERT( m_socket != NULL ) ;
			WAWO_ASSERT( message->GetType() == wawo::net::message::Wawo::T_NONE );

			message->SetType( wawo::net::message::Wawo::T_REQUEST );

			RequestedMessage requested = { m_socket, message };
			//response could arrive before enqueue, so we must push_back before send_packet
			{
				LockGuard<SpinMutex> _lg( m_requested_mutex );
				m_requested.push_back( requested );
			}

			int rt = MyBasePeerT::DoSend( m_socket, message );
			if( rt != wawo::OK ) {
				LockGuard<SpinMutex> _lg( m_requested_mutex );
				typename RequestedMessagePool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&]( RequestedMessage const& req ){
					return req.message->GetNetId() == message->GetNetId();
				});
				WAWO_ASSERT( it != m_requested.end() );
				m_requested.erase( it );
			}

			return rt;
		}

		int Respond( WAWO_SHARED_PTR<MyMessageT> const& response, WAWO_SHARED_PTR<MyMessageT> const& original, MyPeerCtxInfoT const& ctx ) {

			WAWO_ASSERT( original != NULL );
			WAWO_ASSERT( response != NULL );
			WAWO_ASSERT( response->GetType() == wawo::net::message::Wawo::T_NONE );
			WAWO_ASSERT( original->GetType() == wawo::net::message::Wawo::T_REQUEST );

			WAWO_ASSERT( ctx.peer == this );
			WAWO_ASSERT( ctx.socket != NULL );
			//WAWO_ASSERT( ctx.socket == m_socket );

			response->SetType( wawo::net::message::Wawo::T_RESPONSE );
			response->SetNetId( original->GetNetId() );

			return MyBasePeerT::DoSend( ctx.socket, response );
		}

		void HandleIncomingMessage( WAWO_REF_PTR<MySocketT> const& socket, WAWO_SHARED_PTR<MyMessageT> const& message ) {

			WAWO_ASSERT( MyBasePeerT::GetProxy() != NULL );

			switch( message->GetType() ) {
			case wawo::net::message::Wawo::T_SEND:
			case wawo::net::message::Wawo::T_REQUEST:
				{
					MyPeerCtxInfoT ctx;
					ctx.peer = WAWO_REF_PTR< MyBasePeerT>( this );
					ctx.socket = socket;

					MyBasePeerT::GetProxy()->HandleMessage( message, ctx );
				}
				break;
			case wawo::net::message::Wawo::T_RESPONSE:
				{
					bool bTrigger = false;
					RequestedMessage req;
					{
						LockGuard<SpinMutex> _lg( m_requested_mutex );
						typename RequestedMessagePool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&]( RequestedMessage const& req ){
							return req.message->GetNetId() == message->GetNetId();
						});
						WAWO_ASSERT( it != m_requested.end() );

						WAWO_ASSERT( it->socket == socket );

						//make a copy
						WAWO_SHARED_PTR<Packet> arrive_packet( new Packet(*message->GetPacket()) );
						WAWO_REF_PTR<Event> evt ( new Event( wawo::net::message::Wawo::E_RESPONSE, arrive_packet ));

						if( !((*it).message->TriggerEvent( evt )) ) {
							bTrigger = true;
							req = *it;
						}

						m_requested.erase ( it );
					}

					if(bTrigger) {
						WAWO_ASSERT( req.socket != NULL );
						WAWO_ASSERT( req.message != NULL );

						MyPeerCtxInfoT ctx;
						ctx.peer = WAWO_REF_PTR<MyBasePeerT>( this );
						ctx.socket = socket;
						ctx.message = req.message;

						MyBasePeerT::GetProxy()->HandleMessage( message, ctx );
					}
				}
				break;
			default:
				{
					WAWO_THROW_EXCEPTION("invalid message type");
				}
				break;
			}
		}

		virtual void HandleDisconnected( WAWO_REF_PTR<MySocketT> const& socket, int const& ec ) {
			MyPeerCtxInfoT ctx;
			ctx.peer = WAWO_REF_PTR<MyBasePeerT>( this );
			MyBasePeerT::GetProxy()->HandleDisconnected( socket, ctx, ec );
		}

		virtual void HandleError( WAWO_REF_PTR<MySocketT> const& socket, int const& ec ) {
			MyPeerCtxInfoT ctx;
			ctx.peer = WAWO_REF_PTR<MyBasePeerT>( this );
			MyBasePeerT::GetProxy()->HandleError( socket, ctx , ec );
		}

		/*
		void Echo_RequestPing() {
		}
		void Echo_HandlePong(WAWO_REF_PTR<MessageT> const& incoming, WAWO_REF_PTR<MessageT> const& original) {
		}
		*/

		int Echo_RequestHello () {

			WAWO_SHARED_PTR<Packet> packet( new Packet(256) );
			std::string hello_string = "hello server";

			packet->Write<uint32_t>( wawo::net::service::C_ECHO_HELLO );
			packet->Write<uint32_t>( hello_string.length() );
			packet->Write((byte_t const* const)hello_string.c_str(), hello_string.length() );

			WAWO_SHARED_PTR<MyMessageT> message( new MyMessageT( wawo::net::WSI_ECHO, packet ) );

			int rt = Request( message );
			WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			return rt;

//#define TEST_EPOLLIN_EVT
#ifdef TEST_EPOLLIN_EVT
			const int bytes_len = 4096;
			byte_t _bytes[bytes_len] = {0};
			for( int i=0;i<bytes_len;i++ ) {
				_bytes[i] = '\0' + i;
			}

			WAWO_REF_PTR<Packet> packet_o( new Packet(2048) );
			packet_o->Write(wawo::net::service::C_ECHO_HELLO);
			packet_o->Write<uint32_t>(bytes_len);
			packet_o->Write( &_bytes[0],bytes_len );

			WAWO_REF_PTR<NetMessageType> message_s( new NetMessageType( wawo::net::WSI_ECHO, packet_o ) );

			int i = 0;
			do {
				i++;
				rt = Send(message_s);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

	//			Packet pack_t1 = *packet_o;
				WAWO_REF_PTR<Packet> packet_t1( new Packet( *packet_o ) );
				WAWO_REF_PTR<NetMessageType> message_r1( new NetMessageType( wawo::net::WSI_ECHO, packet_t1 ) );
	//			rt = Request(message_r1);
	//			WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				rt = Send(message_s);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				rt = Send(message_s);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				WAWO_REF_PTR<Packet> packet_t2( new Packet( *packet_o ) );
				WAWO_REF_PTR<NetMessageType> message_r2( new NetMessageType( wawo::net::WSI_ECHO, packet_t2 ) );
	//			rt = Request(message_r2);
	//			WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				rt = Send(message_s);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				if( i == 1000) {
					WAWO_REF_PTR<Socket> const& socket = m_endpoint->GetConnection()->GetSocket();
					//socket->Shutdown(Socket::SSHUT_WR);
				}

			}while(i<120000000000);

	#endif

		}

	};

}}}

#endif
