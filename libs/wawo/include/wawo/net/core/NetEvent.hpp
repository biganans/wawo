#ifndef _WAWO_NET_CORE_NET_EVENT_HPP_
#define _WAWO_NET_CORE_NET_EVENT_HPP_

#include <wawo/net/core/Event.hpp>

namespace wawo { namespace net { namespace core {

	struct SocketObserverEvent {
		enum _Event {
			EVT_READ = 0x01, //check read, sys io
			EVT_WRITE = 0x02, //check write, sys io
			EVT_ERROR = 0x04, //error, sys io
			EVT_CLOSE = 0x08, //epoll, sys ip, RDHUB
			EVT_DELAY_WRITE = 0x10,//delay by socket, custom io
			EVT_DELAY_READ_BY_OBSERVER = 0x20, //custom io
			EVT_DELAY_WRITE_BY_OBSERVER = 0x40, //custom io
		};

		enum _EventSets {
			EVT_RD		= (EVT_READ|EVT_DELAY_READ_BY_OBSERVER),
			EVT_WR		= (EVT_WRITE|EVT_DELAY_WRITE|EVT_DELAY_WRITE_BY_OBSERVER),
			EVT_RDWR	= (EVT_RD|EVT_WR),
			EVT_ALL		= (EVT_RDWR|EVT_ERROR|EVT_CLOSE)
		};
	};

	enum NetEvent {

		//io event
		IE_CONNECTED=1,
		IE_CAN_ACCEPT,
		IE_CAN_READ,
		IE_CAN_WRITE,
		IE_ERROR,
		IE_SHUTDOWN_RD,
		IE_SHUTDOWN_WR,
		IE_SHUTDOWN,
		IE_CLOSE,

		//socket evt
		SE_CONNECTED, //10
		SE_ACCEPTED, //11
		SE_READ, //12
		SE_WRITE, //13
		SE_PACKET_ARRIVE,//14
		SE_PACKET_SENT,//15
		SE_SEND_BLOCK,//16
		SE_ERROR, //17
		SE_SHUTDOWN_RD, //18,
		SE_SHUTDOWN_WR, //19
		SE_SHUTDOWN, //20
		SE_CLOSE, //21
		
		NE_MAX
	};

	template <class _SocketT>
	class SocketObserver;

	template <class _SocketT>
	class IOEvent:
		public Event
	{
		typedef IOEvent<_SocketT> MyT;
		typedef SocketObserver<_SocketT> MySocketObserverT;
	public:
		explicit IOEvent( WAWO_REF_PTR<MySocketObserverT> const& observer, uint32_t const& id, EventData const& data ) :
			Event(id,data),
			m_observer(observer)
		{
		}

		inline WAWO_REF_PTR<MySocketObserverT>& GetObserver() { return m_observer ; }
		inline WAWO_REF_PTR<MySocketObserverT> const& GetObserver() const { return m_observer ; }
	private:
		WAWO_REF_PTR<MySocketObserverT> m_observer;
	};

	template <class _PacketProtocolT>
	class BufferSocket;

	template <class _PacketProtocolT>
	class SocketEvent:
		public Event
	{

	public:
		typedef BufferSocket<_PacketProtocolT> MySocketT;
		typedef SocketEvent<_PacketProtocolT> MyT;

		explicit SocketEvent( WAWO_REF_PTR<MySocketT> const& socket, uint32_t const& id ) :
			Event(id),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WAWO_REF_PTR<MySocketT> const& socket, uint32_t const& id, WAWO_SHARED_PTR<Packet> const& packet ) :
			Event(id,packet),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WAWO_REF_PTR<MySocketT> const& socket, uint32_t const& id, EventData const& data ) :
			Event(id,data),
			m_socket(socket)
		{
		}
		explicit SocketEvent( WAWO_REF_PTR<MySocketT> const& socket, uint32_t const& id,WAWO_SHARED_PTR<Packet> const& packet, EventData const& data ) :
			Event(id,packet,data),
			m_socket(socket)
		{
		}

		inline WAWO_REF_PTR<MySocketT>& GetSocket() { return m_socket ; }
		inline WAWO_REF_PTR<MySocketT> const& GetSocket() const { return m_socket ; }
	private:
		WAWO_REF_PTR<MySocketT> m_socket;
	};
}}}

#endif