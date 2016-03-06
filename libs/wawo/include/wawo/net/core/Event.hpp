#ifndef _WAWO_NET_CORE_EVENT_HPP_
#define _WAWO_NET_CORE_EVENT_HPP_

#include <wawo/core.h>

#include <wawo/SmartPtr.hpp>
#include <wawo/NonCopyable.hpp>

#include <wawo/algorithm/Packet.hpp>


namespace wawo { namespace net { namespace core {

	using namespace wawo::algorithm;
	typedef wawo::Data64 EventData;
	
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