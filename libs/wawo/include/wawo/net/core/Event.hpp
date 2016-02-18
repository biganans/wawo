#ifndef _WAWO_NET_CORE_EVENT_HPP_
#define _WAWO_NET_CORE_EVENT_HPP_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/NonCopyable.hpp>

#include <wawo/algorithm/Packet.hpp>

namespace wawo { namespace net { namespace core {

	using namespace wawo::algorithm;

	union EventData {
		explicit EventData() :int64_v(0){}
		explicit EventData(uint64_t const& u64_v) : uint64_v(u64_v){}
		explicit EventData(uint32_t const& u32_v) : uint32_v(u32_v){}
		explicit EventData(uint16_t const& u16_v) : uint16_v(u16_v){}
		explicit EventData(uint8_t const& u8_v) : uint8_v(u8_v){}
		explicit EventData(int64_t const& i64_v) : int64_v(i64_v){}
		explicit EventData(int32_t const& i32_v) : int32_v(i32_v){}
		explicit EventData(int16_t const& i16_v) : int16_v(i16_v){}
		explicit EventData(int8_t const& i8_v) : int8_v(i8_v){}
		explicit EventData( void* const ptr) : ptr_v(ptr) {}

		uint64_t uint64_v;
		uint32_t uint32_v;
		uint16_t uint16_v;
		uint8_t uint8_v;

		int64_t int64_v;
		int32_t int32_v;
		int16_t int16_v;
		int8_t int8_v;

		void* ptr_v;
	};
	
	//root class for dispatchable object
	class Event :
		public wawo::RefObject_Abstract
	{
		
	public:
 		explicit Event( uint32_t const& id ):
			m_id( id ),
			m_packet(NULL),
			m_data()
		{
		}
		
		explicit Event( uint32_t const& id, EventData const& data ):
			m_id( id ),
			m_packet(NULL),
			m_data(data)
		{
		}
		
		explicit Event( uint32_t const& id, WAWO_SHARED_PTR<Packet> const& packet, EventData const& data ):
			m_id( id ),
			m_packet(packet),
			m_data(data)
		{
		}

		explicit Event( uint32_t const& id, WAWO_SHARED_PTR<Packet> const& packet ):
			m_id( id ),
			m_packet(packet),
			m_data()
		{
		}

		~Event() {
#ifdef _DEBUG
			WAWO_ASSERT( m_id != 0xFFFFFFFF ) ;
			m_id = 0xFFFFFFFF;
			m_packet = NULL;
#endif
		}
		
		inline void SetId( uint32_t const& id) { m_id = id; }
		inline uint32_t const& GetId() { return m_id; }
		inline uint32_t const& GetId() const { return m_id; }

		inline void SetEventData(EventData const& data) {m_data = data;}
		inline EventData const& GetEventData() { return m_data; }
		inline EventData const& GetEventData() const { return m_data; }

		void SwapPacket( WAWO_SHARED_PTR<Packet>& packet ) {
			m_packet.swap(packet);
		}

		inline WAWO_SHARED_PTR<Packet> GetPacket() const {
			return m_packet;
		}

		inline void SetPacket( WAWO_SHARED_PTR<Packet> const& packet ) {
			m_packet = packet;
		}
		
	private:
		uint32_t					m_id;
		WAWO_SHARED_PTR<Packet>		m_packet;
		EventData					m_data;
	};

}}}
#endif