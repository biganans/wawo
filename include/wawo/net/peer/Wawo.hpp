#ifndef _WAWO_NET_PEER_WAWO_HPP_
#define _WAWO_NET_PEER_WAWO_HPP_

#include <wawo/thread/Mutex.hpp>

#include <wawo/net/core/TLP/HLenPacket.hpp>
#include <wawo/net/peer/message/Wawo.hpp>
#include <wawo/net/Peer_Abstract.hpp>

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

	 *
	 * wawo's peer impl do not have a status of login,this is left for yourself
	 * there are several ways to impl a login logic, if you have no clue where to start, pls read the lines below:

	 * First solution
	 * 1, extend wawo::net::peer::Wawo, add a state of login|logout, impl your own login message, switch to login state when you get right login response success
	   2, check this state for every message out and in

	 * Seconds solution
	 * 1, add kinds authorize message, once one authorize for some kinds of access is granted, add it to your permisson available table
	 * 2, check permission scope for each message out and in
	 */

	struct WawoCallback_Abstract :
		public wawo::RefObject_Abstract
	{
		virtual bool OnRespond(WWSP<wawo::algorithm::Packet> const& packet) = 0;
		virtual bool OnError(int const& ec) = 0;
	};

	struct RequestedMessage
	{
		WWSP<message::Wawo> message;
		WWRP<wawo::net::peer::WawoCallback_Abstract> cb;
		u64_t ts_request;
		u32_t timeout;
	};

	template <class _TLP = TLP::HLenPacket, int __=0>
	class Wawo:
		public Peer_Abstract,
		public Listener_Abstract< SocketEvent >,
		public Dispatcher_Abstract< PeerEvent< Wawo<_TLP,__> > >
	{

	public:
		typedef _TLP TLPT;
		typedef Listener_Abstract< SocketEvent > ListenerT;

		typedef message::Wawo MessageT;
		typedef Wawo<TLPT,__> MyT;

		typedef PeerEvent<MyT> PeerEventT;
		typedef Dispatcher_Abstract< PeerEventT > DispatcherT;

	private:
		typedef std::vector< RequestedMessage > RequestedMessagePool ;

		SpinMutex m_requested_mutex;
		RequestedMessagePool m_requested ;

		SharedMutex m_mutex;
		WWRP<Socket> m_socket;

	public:
		explicit Wawo()
		{
		}

		virtual ~Wawo() {
			_CancelAllRequest();
		}

		void _CancelAllRequest( int const& ec = 0 ) {
			LockGuard<SpinMutex> _lg( m_requested_mutex );
			std::for_each( m_requested.begin(), m_requested.end(), [&ec]( RequestedMessage const& req ) {
				wawo::net::peer::WawoCallback_Abstract* cb = req.cb.Get() ;

				if(cb != NULL) {
					cb->OnError(ec);
				}
			});
		}

		void _CheckRequestedMessage() {
		}

		virtual void Tick() {
			_CheckHeartBeat();
			_CheckRequestedMessage();
		}

		void _CheckHeartBeat() {
		 //@to impl
		}

		int Send( WWSP<MessageT> const& message ) {

			WAWO_ASSERT( message != NULL);
			WAWO_ASSERT( message->GetType() == wawo::net::peer::message::Wawo::T_NONE );

			message->SetType( wawo::net::peer::message::Wawo::T_SEND );
			return MyT::DoSendMessage( message );
		}

		int Request(WWSP<MessageT> const& message) {
			//empty cb
			return Request( message, WWRP<wawo::net::peer::WawoCallback_Abstract>(NULL) );
		}

		int Request( WWSP<MessageT> const& message, WWRP<wawo::net::peer::WawoCallback_Abstract> const& cb, u32_t const& timeout = 15 /*in seconds*/ ) {
			WAWO_ASSERT( message != NULL);
			WAWO_ASSERT( message->GetType() == wawo::net::peer::message::Wawo::T_NONE );
			message->SetType( wawo::net::peer::message::Wawo::T_REQUEST );

			RequestedMessage requested = { message, cb, wawo::time::curr_milliseconds(), timeout*1000 };
			//response could arrive before enqueue, so we must push_back before send_packet
			{
				LockGuard<SpinMutex> _lg( m_requested_mutex );
				m_requested.push_back( requested );
			}

			int msndrt = MyT::DoSendMessage( message );
			WAWO_RETURN_V_IF_MATCH(msndrt, msndrt == wawo::OK);

			LockGuard<SpinMutex> _lg( m_requested_mutex );
			typename RequestedMessagePool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&message]( RequestedMessage const& req ){
				return req.message == message;
			});
			WAWO_CONDITION_CHECK( it != m_requested.end() );
			m_requested.erase( it );

			return msndrt;
		}

		int Respond( WWSP<MessageT> const& response, WWSP<MessageT> const& incoming ) {

			WAWO_ASSERT( response != NULL);
			WAWO_ASSERT( incoming != NULL);

			WAWO_ASSERT( response->GetType() == wawo::net::peer::message::Wawo::T_NONE );
			WAWO_ASSERT( incoming->GetType() == wawo::net::peer::message::Wawo::T_REQUEST );

			response->SetType( wawo::net::peer::message::Wawo::T_RESPONSE );
			response->SetNetId( incoming->GetNetId() );

			return MyT::DoSendMessage( response );
		}

		virtual void OnEvent( WWRP<SocketEvent> const& evt) {

			WAWO_ASSERT(evt->GetSocket() == m_socket );
			u32_t const& id = evt->GetId();
			int const& ec = evt->GetCookie().int32_v;

			switch( id ) {

			case SE_PACKET_ARRIVE:
				{
					WWSP<Packet> const& inpack = evt->GetPacket();

					int message_parse_ec;
					WWSP<MessageT> arrives[5];
					u32_t count = _DecodeMessageFromPacket(inpack, arrives, 5, message_parse_ec);
					if (message_parse_ec != wawo::OK) {
						WAWO_WARN("[peer_wawo]message parse error: %d", message_parse_ec);
					}
					WAWO_ASSERT(count <= 5);

					//parse message from packet, and trigger message
					for (u32_t i = 0; i<count; i++) {
						_HandleIncomingMessage(evt->GetSocket(), arrives[i]);
					}
				}
				break;
			case SE_CONNECTED:
				{
					WAWO_ASSERT(evt->GetSocket() == m_socket);
					WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec ));
					DispatcherT::Trigger(pevt1);

					WWRP<PeerEventT> pevt2(new PeerEventT(PE_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt2);

					evt->GetSocket()->UnRegister(SE_ERROR, WWRP<ListenerT>(this) );
				}
				break;
			case SE_RD_SHUTDOWN:
				{
					WWRP<PeerEventT> pevt( new PeerEventT( PE_SOCKET_RD_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket() , evt->GetCookie().int32_v ) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case SE_WR_SHUTDOWN:
				{
					WWRP<PeerEventT> pevt( new PeerEventT( PE_SOCKET_WR_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket() , evt->GetCookie().int32_v ) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case SE_CLOSE:
				{
					_CancelAllRequest(ec);
					DetachSocket(evt->GetSocket());

					WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CLOSE, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt1);

					WWRP<PeerEventT> pevt2(new PeerEventT(PE_CLOSE, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt2);
				}
				break;
			case SE_ERROR:
				{
					WWRP<PeerEventT> pevt(new PeerEventT(PE_SOCKET_ERROR, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt);
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

		int DoSendMessage( WWSP<MessageT> const& message ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if( m_socket == NULL ) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<Packet> packet_mo;
			int encode_ec = message->Encode( packet_mo ) ;

			if( encode_ec != wawo::OK ) {
				return encode_ec;
			}

			WAWO_ASSERT(packet_mo != NULL);
			return m_socket->SendPacket(packet_mo);
		}

		int DoReceiveMessages(WWSP<MessageT> messages[], u32_t const& size, int& ec) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if (m_socket == NULL) {
				ec = wawo::E_PEER_NO_SOCKET_ATTACHED;
				return 0;
			}

			WWSP<Packet> arrives[1];
			u32_t pcount;
			do {
				pcount = m_socket->ReceivePackets(arrives, 1, ec);
			} while ((ec == wawo::OK || ec == wawo::E_SOCKET_RECV_IO_BLOCK) && pcount == 0);
			WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);

			WAWO_ASSERT(pcount == 1);
			WAWO_ASSERT(arrives[0]->Length()>0);

			int message_parse_ec;
			u32_t mcount = _DecodeMessageFromPacket(arrives[0], messages, size, message_parse_ec);
			if (message_parse_ec != wawo::OK) {
				WAWO_WARN("[peer_wawo]message parse error: %d", message_parse_ec);
			}
			WAWO_ASSERT(mcount <= size);
			return mcount;
		}

		void AttachSocket( WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			WAWO_ASSERT( m_socket == NULL );
			WAWO_ASSERT( socket != NULL );

			m_socket = socket;
			WWRP<ListenerT> peer_l( this );
			socket->Register(SE_PACKET_ARRIVE, peer_l );
			socket->Register(SE_CONNECTED, peer_l);
			socket->Register(SE_CLOSE, peer_l);
			socket->Register(SE_RD_SHUTDOWN, peer_l);
			socket->Register(SE_WR_SHUTDOWN, peer_l);
			socket->Register(SE_ERROR, peer_l);
		}

		void DetachSocket(WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);

			if ( m_socket != NULL ) {
				WAWO_ASSERT( socket == m_socket );
				WWRP<ListenerT> peer_l( this );
				m_socket->UnRegister( peer_l );
				m_socket = NULL;
			}
		}

		int Close( int const& code = 0) {
			LockGuard<SharedMutex> lg(m_mutex);
			if ( m_socket != NULL ) {
				return m_socket->Close(code);
			}
			return wawo::OK;
		}

		void GetSockets( std::vector< WWRP<Socket> >& sockets ) {
			SharedLockGuard<SharedMutex> slg( m_mutex);
			if( m_socket != NULL ) {
				sockets.push_back( m_socket );
			}
		}

		virtual bool HaveSocket(WWRP<Socket> const& socket) {
			SharedLockGuard<SharedMutex> slg( m_mutex);
			return m_socket == socket;
		}

		inline u32_t _DecodeMessageFromPacket( WWSP<Packet> const& inpack, WWSP<message::Wawo> messages_o[], u32_t const& size, int& ec_o ) {
			ec_o = wawo::OK;
			u32_t count = 0 ;
			do {

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
				u32_t mlen = (inpack->Read<wawo::u16_t>()&0xFFFF) ;
#else
				u32_t mlen = inpack->Read<wawo::u32_t>() ;
#endif
				WAWO_CONDITION_CHECK(inpack->Length() >= mlen ) ;

				if( mlen == inpack->Length() ) {
					message::Wawo::NetIdT net_id = inpack->Read<message::Wawo::NetIdT>();
					message::Wawo::Type type = (message::Wawo::Type)(inpack->Read<message::Wawo::TypeT>());
					//for the case on net_message one packet
					messages_o[count++] = WWSP<message::Wawo> ( new message::Wawo(net_id,type, inpack) );
					break;
				} else {
					//one packet , mul-messages
					//copy and assign
					WAWO_ASSERT( "@TODO" );
				}
			} while(inpack->Length() && (count<size) );

			return count ;
		}

		void _HandleIncomingMessage( WWRP<Socket> const& socket, WWSP<MessageT> const& message ) {

			switch( message->GetType() ) {
			case wawo::net::peer::message::Wawo::T_SEND:
			case wawo::net::peer::message::Wawo::T_REQUEST:
				{
					WWRP<PeerEventT> pevt( new PeerEventT( PE_MESSAGE, WWRP<MyT>(this), socket, message ) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case wawo::net::peer::message::Wawo::T_RESPONSE:
				{
					bool bStopPropagation ;
					WWSP<MessageT> requested_message;
					RequestedMessage req;
					{
						LockGuard<SpinMutex> _lg( m_requested_mutex );
						typename RequestedMessagePool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&message]( RequestedMessage const& req ){
							return req.message->GetNetId() == message->GetNetId();
						});
						WAWO_ASSERT( it != m_requested.end() );

						//make a copy
						WWSP<Packet> arrive_packet( new Packet(*message->GetPacket()) );
						//WWRP<Event> evt ( new Event( message::Wawo::ME_RESPONSE, arrive_packet ));

						//trigger cb
						wawo::net::peer::WawoCallback_Abstract* cb = it->cb.Get() ;

						if( cb != NULL && cb->OnRespond( arrive_packet ) ) {
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

						WWRP<PeerEventT> pevt( new PeerEventT( PE_MESSAGE, WWRP<MyT>(this), socket, message, requested_message ) );
						DispatcherT::Trigger(pevt);
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

		/*
		int Echo_RequestHello () {

			WWSP<Packet> packet( new Packet(256) );
			std::string hello_string = "hello server";

			packet->Write<u32_t>( services::C_ECHO_HELLO );
			packet->Write<u32_t>( hello_string.length() );
			packet->Write((byte_t const* const)hello_string.c_str(), hello_string.length() );

			packet->WriteLeft<wawo::net::ServiceIdT>(services::S_ECHO);
			WWSP<MessageT> message( new MessageT( packet ) );

			int rt = Request( message );
			WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			return rt;
		}
		*/

	};
}}}
#endif
