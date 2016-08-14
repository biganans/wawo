#ifndef _WAWO_NET_PEER_CARGO_HPP
#define _WAWO_NET_PEER_CARGO_HPP

#include <wawo/net/core/TLP/HLenPacket.hpp>
#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/core/Dispatcher_Abstract.hpp>

#include <wawo/net/Peer_Abstract.hpp>
#include <wawo/net/peer/message/Packet.hpp>

namespace wawo { namespace net { namespace peer {

	using namespace wawo::algorithm;
	using namespace wawo::thread;
	using namespace wawo::net;

	template <class _TLP = core::TLP::HLenPacket>
	class Cargo:
		public Peer_Abstract,
		public core::Listener_Abstract<SocketEvent>,
		public core::Dispatcher_Abstract< PeerEvent< Cargo<_TLP> > >
	{

	public:
		typedef _TLP TLPT;
		typedef Cargo<TLPT> MyT;
		typedef core::Listener_Abstract<SocketEvent> ListenerT;
		typedef core::Dispatcher_Abstract< PeerEvent< MyT > > DispatcherT;
	public:
		typedef message::PacketMessage MessageT;
		typedef PeerEvent< MyT > PeerEventT;

	private:
		SharedMutex m_mutex;
		WWRP<Socket> m_socket;

	public:
		Cargo() {}
		virtual ~Cargo() {}

		virtual void AttachSocket( WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			WAWO_ASSERT( socket != NULL );
			WAWO_ASSERT( m_socket == NULL );

			WWRP<ListenerT> peer_l( this );
			socket->Register(SE_PACKET_ARRIVE, peer_l);
			socket->Register(SE_CONNECTED, peer_l);
			socket->Register(SE_CLOSE, peer_l);
			socket->Register(SE_RD_SHUTDOWN, peer_l);
			socket->Register(SE_WR_SHUTDOWN, peer_l);
			socket->Register(SE_ERROR, peer_l);
			m_socket = socket;
		}

		virtual void DetachSocket( WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			WAWO_ASSERT( socket != NULL );

			if( m_socket != NULL ) {
				WAWO_ASSERT( socket == m_socket );
				WWRP<ListenerT> peer_l( this );
				m_socket->UnRegister( peer_l );
				m_socket = NULL;
			}
		}


		virtual void GetSockets( std::vector< WWRP<Socket> >& sockets ) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if( m_socket != NULL ) {
				sockets.push_back(m_socket);
			}
		}

		virtual bool HaveSocket(WWRP<Socket> const& socket) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			return m_socket == socket;
		}

		WWRP<Socket> GetSocket() {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			return m_socket;
		}

		virtual int Close(int const& close_code = 0 ) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if( m_socket != NULL ) {
				return m_socket->Close(close_code);
			}
			return wawo::OK;
		}

		virtual

		int DoSendMessage( WWSP<MessageT> const& message ) {
			SharedLockGuard<SharedMutex> slg(m_mutex);

			if(m_socket== NULL) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<Packet> packet_mo;
			int encode_ec = message->Encode( packet_mo ) ;

			if( encode_ec != wawo::OK ) {
				return encode_ec;
			}

			WAWO_ASSERT( packet_mo != NULL );
			return m_socket->SendPacket( packet_mo );
		}

		int DoReceiveMessages(WWSP<MessageT> message[], u32_t const& size, int& ec ) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if (m_socket == NULL) {
				ec = wawo::E_PEER_NO_SOCKET_ATTACHED;
				return 0;
			}

			WWSP<Packet> arrives[1];
			u32_t pcount;
			do {
				pcount = m_socket->ReceivePackets(arrives, 1, ec);
			} while ((ec==wawo::OK || ec == wawo::E_SOCKET_RECV_IO_BLOCK)&&pcount==0);
			WAWO_RETURN_V_IF_NOT_MATCH(ec, ec==wawo::OK );

			WAWO_ASSERT( pcount == 1 );
			WAWO_ASSERT(arrives[0]->Length()>0 );

			WWSP<MessageT> _m(new MessageT(arrives[0]));
			message[0] = _m;
			return 1;
		}

		virtual void OnEvent( WWRP<SocketEvent> const& evt) {

			u32_t const& id = evt->GetId();
			int const& ec = evt->GetCookie().int32_v;

			switch( id ) {

			case SE_PACKET_ARRIVE:
				{
					WWSP<Packet> const& inpack = evt->GetPacket();
					WWRP<Socket> const& socket = evt->GetSocket();

					WWSP<MessageT> message( new MessageT(inpack) );
					WWRP<PeerEventT> pevt( new PeerEventT(PE_MESSAGE, WWRP<MyT>(this), socket, message) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case SE_RD_SHUTDOWN:
				{
					WWRP<PeerEventT> pevt( new PeerEventT( PE_SOCKET_RD_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket() , ec ) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case SE_WR_SHUTDOWN:
				{
					WWRP<PeerEventT> pevt( new PeerEventT( PE_SOCKET_WR_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket() , ec ) );
					DispatcherT::Trigger(pevt);
				}
				break;
			case SE_CLOSE:
				{
					WAWO_ASSERT(evt->GetSocket() == m_socket);

					DetachSocket(evt->GetSocket());
					WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CLOSE, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt1);

					WWRP<PeerEventT> pevt2( new PeerEventT(PE_CLOSE, WWRP<MyT>(this), evt->GetSocket() , ec ) );
					DispatcherT::Trigger(pevt2);
				}
				break;
			case SE_CONNECTED:
				{
					WAWO_ASSERT( evt->GetSocket() == m_socket );
					WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt1);

					WWRP<PeerEventT> pevt2(new PeerEventT(PE_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec));
					DispatcherT::Trigger(pevt2);

					evt->GetSocket()->UnRegister(SE_ERROR, WWRP<ListenerT>(this));
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
	};

}}}
#endif