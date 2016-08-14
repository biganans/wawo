#ifndef _WAWO_NET_CORE_EVENT_HPP_
#define _WAWO_NET_CORE_EVENT_HPP_

#include <wawo/core.h>
#include <wawo/algorithm/Packet.hpp>

namespace wawo { namespace net { namespace core {

	using namespace wawo::algorithm;
	typedef wawo::UData64 Cookie;

	//root class for dispatchable object
	class Event :
		virtual public wawo::RefObject_Abstract
	{

	public:
 		explicit Event( u32_t const& id ):
			m_id( id ),
			m_packet(NULL),
			m_cookie()
		{
		}

		explicit Event( u32_t const& id, WWSP<Packet> const& packet ):
			m_id( id ),
			m_packet(packet),
			m_cookie()
		{
		}

		explicit Event( u32_t const& id, Cookie const& cookie ):
			m_id( id ),
			m_packet(NULL),
			m_cookie(cookie)
		{
		}
		explicit Event( u32_t const& id, WWSP<Packet> const& packet, Cookie const& cookie ):
			m_id( id ),
			m_packet(packet),
			m_cookie(cookie)
		{
		}

		~Event() {
#ifdef _DEBUG
			WAWO_ASSERT( m_id != 0xFFFFFFFF ) ;
			m_id = 0xFFFFFFFF;
			m_packet = NULL;
#endif
		}

		inline void SetId( u32_t const& id) { m_id = id; }
		inline u32_t const& GetId() const { return m_id; }

		inline void SetCookie(Cookie const& data) {m_cookie = data;}
		inline Cookie const& GetCookie() const { return m_cookie; }

		inline WWSP<Packet> GetPacket() const { return m_packet; }
		inline void SetPacket( WWSP<Packet> const& packet ) { m_packet = packet;}

	private:
		u32_t			m_id;
		WWSP<Packet>	m_packet;
		Cookie			m_cookie;
	};
}}}
#endif