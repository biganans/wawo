#ifndef _WAWO_NET_NETEVENT_HPP_
#define _WAWO_NET_NETEVENT_HPP_

#include <wawo/net/core/Event.hpp>

namespace wawo { namespace net {

	enum IOEvent {
		IOE_READ					= 0x01, //check read, sys io
		IOE_WRITE					= 0x02, //check write, sys io
		IOE_ERROR					= 0x04, //error, sys io
		IOE_DELAY_WRITE				= 0x08, //delay by socket, custom io
		IOE_DELAY_READ_BY_OBSERVER	= 0x10, //custom io
		IOE_DELAY_WRITE_BY_OBSERVER	= 0x20, //custom io
	};

	enum IOEventSets {
		IOE_RD		= (IOE_READ|IOE_DELAY_READ_BY_OBSERVER),
		IOE_WR		= (IOE_WRITE|IOE_DELAY_WRITE|IOE_DELAY_WRITE_BY_OBSERVER),
		IOE_RDWR	= (IOE_RD|IOE_WR),
		IOE_ALL		= (IOE_RDWR|IOE_ERROR )
	};

	enum NetEvent {

		//io event
		IE_CONNECTED=1,
		IE_CAN_ACCEPT,
		IE_CAN_READ,
		IE_CAN_WRITE,
		IE_ERROR,

		//socket evt
		SE_CONNECTED, //6
		SE_ACCEPTED, //7
		SE_TLP_HANDSHAKE_DONE, //8
		SE_PACKET_ARRIVE,//9
		SE_PACKET_SENT,//10
		SE_SEND_BLOCKED,//11
		SE_ERROR, //12
		SE_RD_SHUTDOWN, //13,
		SE_WR_SHUTDOWN, //14
		SE_CLOSE, //15

		PE_SOCKET_CONNECTED, //16
		PE_SOCKET_RD_SHUTDOWN, //17
		PE_SOCKET_WR_SHUTDOWN, //18
		PE_SOCKET_CLOSE, //19
		PE_SOCKET_ERROR, //20
		PE_MESSAGE, //21
		PE_CONNECTED, //22
		PE_CLOSE, //23
		
		NE_MAX
	};

	class Socket;

	class SocketEvent :
		public core::Event
	{

	public:
		explicit SocketEvent( WWRP<Socket> const& socket, u32_t const& id ) :
			Event(id),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WWRP<Socket> const& socket, u32_t const& id, WWSP<wawo::algorithm::Packet> const& packet ) :
			Event(id,packet),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WWRP<Socket> const& socket, u32_t const& id, core::Cookie const& data ) :
			Event(id,data),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WWRP<Socket> const& socket, u32_t const& id,WWSP<wawo::algorithm::Packet> const& packet, core::Cookie const& data ) :
			Event(id,packet,data),
			m_socket(socket)
		{
		}

		inline WWRP<Socket> const& GetSocket() const { return m_socket ; }

	private:
		WWRP<Socket> m_socket;
	};

	template <class _PeerT>
	class PeerEvent :
		public wawo::net::core::Event
	{
		typedef _PeerT PeerT;
		typedef typename _PeerT::MessageT MessageT;
	private:
		WWRP<PeerT> m_peer;
		WWRP<Socket> m_socket;
		WWSP<MessageT> m_incoming; //incoming message

	public:
		explicit PeerEvent(u32_t const& id, WWRP<PeerT> const& peer) :
			Event(id),
			m_peer(peer),
			m_socket(NULL),
			m_incoming(NULL)
		{}
		explicit PeerEvent(u32_t const& id, WWRP<PeerT> const& peer, WWRP<Socket> const& socket) :
			Event(id),
			m_peer(peer),
			m_socket(socket),
			m_incoming(NULL)
		{}
		explicit PeerEvent(u32_t const& id, WWRP<PeerT> const& peer, WWRP<Socket> const& socket, int const& ec) :
			Event(id, core::Cookie(ec)),
			m_peer(peer),
			m_socket(socket),
			m_incoming(NULL)
		{}
		explicit PeerEvent(u32_t const& id, WWRP<PeerT> const& peer, WWRP<Socket> const& socket, WWSP<MessageT> const& incoming) :
			Event(id),
			m_peer(peer),
			m_socket(socket),
			m_incoming(incoming)
		{}
		explicit PeerEvent(u32_t const& id, WWRP<PeerT> const& peer, WWRP<Socket> const& socket, WWSP<MessageT> const& incoming, WWSP<MessageT> const& related) :
			Event(id),
			m_peer(peer),
			m_socket(socket),
			m_incoming(incoming)
		{}
		inline WWRP<PeerT> const& GetPeer() const {
			return m_peer;
		}
		inline WWRP<Socket> const& GetSocket() const {
			return m_socket;
		}
		inline WWSP<MessageT> const& GetIncoming() const {
			return m_incoming;
		}
	};
}}
#endif