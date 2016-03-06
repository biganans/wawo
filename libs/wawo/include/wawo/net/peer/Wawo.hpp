#ifndef _WAWO_NET_PEER_WAWO_HPP_
#define _WAWO_NET_PEER_WAWO_HPP_

#include <wawo/thread/Mutex.h>

#include <wawo/net/core/Event.hpp>
#include <wawo/net/Credential.hpp>

#include <wawo/net/protocol/Wawo.hpp>
#include <wawo/net/message/Wawo.hpp>
#include <wawo/net/Peer_Abstract.hpp>

//for debug
#include <wawo/net/ServiceList.h>

namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;
	using namespace wawo::net;
	using namespace wawo::net::core;

	/*
	 * Note: (u'd better read this, if you have any question about wawo::net::peer::Wawo )
	 *
	 * wawo::net::peer::Wawo is a sub class of Peer_Abstract
	 * Peer_Abstract is is what as it is, a object who can send , receive messages by its sockets
	 * it's API is simple as below

	 * a peer is identified by its credential
	 *
	 * wawo's peer impl do not have a status of login,this is left for yourself
	 * there are several ways to impl a login logic, if you have no clue where to start, pls read the lines below:

	 * First solution
	 * 1, extends wawo::net::peer::Wawo, add a state of login|logout, impl your own login message, switch to login state when you get right login response success
	   2, check this state for every message out and in

	 * Seconds solution
	 * 1, add kinds authorize message, once one authorize for some kinds of access is granted, add it to your permisson available table
	 * 2, check permission scope for each message out and in
	 */

	template <class _CredentialT = Credential, class _MessageT = wawo::net::message::Wawo, class _SocketT = wawo::net::core::BufferSocket<wawo::net::protocol::Wawo> >
	class Wawo :
		public Peer_Abstract<_CredentialT,_MessageT,_SocketT>,
		public Dispatcher_Abstract< wawo::net::PeerEvent< Wawo<_CredentialT, _MessageT,_SocketT> > >
	{

	public:
		typedef _CredentialT MyCredentialT;
		typedef _MessageT MyMessageT;
		typedef _SocketT MySocketT;
		typedef typename _SocketT::SocketEventT MySocketEventT;

		typedef Wawo<_CredentialT,_MessageT,_SocketT> MyT;
		typedef Peer_Abstract<_CredentialT,_MessageT,_SocketT> MyBasePeerT;

		typedef PeerEvent< Wawo<_CredentialT, _MessageT,_SocketT > > MyPeerEventT;
		typedef Dispatcher_Abstract< MyPeerEventT > _MyDispatcherT;

	private:
		struct RequestedMessage
		{
			WAWO_SHARED_PTR<MyMessageT> message;
			WAWO_REF_PTR<wawo::net::message::Wawo::Callback_Abstract> cb;
		};

		typedef std::vector< RequestedMessage > RequestedMessagePool ;

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

		virtual void Tick() {
			_CheckHeartBeat();
		}

		void _CheckHeartBeat() {
		 //@to impl
		}

		int Send( WAWO_SHARED_PTR<MyMessageT> const& message ) {
			WAWO_ASSERT( message != NULL);
			//WAWO_ASSERT( message->GetType() == wawo::net::message::Wawo::T_NONE );

			message->SetType( wawo::net::message::Wawo::T_SEND );
			return MyBasePeerT::DoSend( message );
		}

		int Request(WAWO_SHARED_PTR<MyMessageT> const& message) {
			//empty cb
			return Request( message, WAWO_REF_PTR<message::Wawo::Callback_Abstract>(NULL) );
		}

		int Request( WAWO_SHARED_PTR<MyMessageT> const& message, WAWO_REF_PTR<message::Wawo::Callback_Abstract> const& cb ) {
			WAWO_ASSERT( message != NULL );
			
			//WAWO_ASSERT( message->GetType() == wawo::net::message::Wawo::T_NONE );

			message->SetType( wawo::net::message::Wawo::T_REQUEST );

			RequestedMessage requested = { message, cb };
			//response could arrive before enqueue, so we must push_back before send_packet
			{
				LockGuard<SpinMutex> _lg( m_requested_mutex );
				m_requested.push_back( requested );
			}

			int rt = MyBasePeerT::DoSend( message );
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

		int Respond( WAWO_SHARED_PTR<MyMessageT> const& response, WAWO_SHARED_PTR<MyMessageT> const& incoming ) {

			WAWO_ASSERT( response != NULL );
			//WAWO_ASSERT( response->GetType() == wawo::net::message::Wawo::T_NONE );

			WAWO_ASSERT( incoming != NULL );
			WAWO_ASSERT( incoming->GetType() == wawo::net::message::Wawo::T_REQUEST );

			response->SetType( wawo::net::message::Wawo::T_RESPONSE );
			response->SetNetId( incoming->GetNetId() );

			return MyBasePeerT::DoSend( response );
		}

		void HandleIncomingMessage( WAWO_SHARED_PTR<MyMessageT> const& message ) {

			switch( message->GetType() ) {
			case wawo::net::message::Wawo::T_SEND:
			case wawo::net::message::Wawo::T_REQUEST:
				{
					WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT( PE_MESSAGE, WAWO_REF_PTR<MyT>(this), message ) );
					_MyDispatcherT::Trigger(evt);
				}
				break;
			case wawo::net::message::Wawo::T_RESPONSE:
				{
					bool bStopPropagation ;
					WAWO_SHARED_PTR<MyMessageT> requested_message;
					RequestedMessage req;
					{
						LockGuard<SpinMutex> _lg( m_requested_mutex );
						typename RequestedMessagePool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&]( RequestedMessage const& req ){
							return req.message->GetNetId() == message->GetNetId();
						});
						WAWO_ASSERT( it != m_requested.end() );

						//make a copy
						WAWO_SHARED_PTR<Packet> arrive_packet( new Packet(*message->GetPacket()) );
						//WAWO_REF_PTR<Event> evt ( new Event( message::Wawo::ME_RESPONSE, arrive_packet ));

						//trigger cb
						wawo::net::message::Wawo::Callback_Abstract* cb = it->cb.Get() ;

						if( cb != NULL && (*cb)( message::Wawo::ME_RESPONSE, arrive_packet ) ) {
							bStopPropagation = true;
						} else {
							bStopPropagation = false;
							requested_message = it->message;
						}
						m_requested.erase ( it );
					}

					if(!bStopPropagation) {
						WAWO_ASSERT( socket != NULL );
						WAWO_ASSERT( requested_message != NULL );

						WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT( PE_MESSAGE, WAWO_REF_PTR<MyT>(this), message, requested_message ) );
						_MyDispatcherT::Trigger(evt);
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

#define _TEST_CALLBACK
#ifdef _TEST_CALLBACK
		struct MyCallback:
			public wawo::net::message::Wawo::Callbacks_Abstract
		{
			WAWO_REF_PTR<MyT> peer;
			WAWO_SHARED_PTR<MyMessageT> message; //original message
			MyCallback( WAWO_REF_PTR<MyT> peer, WAWO_SHARED_PTR<MyMessageT> message ):
				peer(peer),
				message(message)
			{
			}

			bool operator()( WAWO_REF_PTR<Event> const& evt ) {
				//...
				return false;
			}
		};
#endif

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
