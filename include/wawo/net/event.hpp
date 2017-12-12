#ifndef _WAWO_NET_EVENT_HPP_
#define _WAWO_NET_EVENT_HPP_

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace net {

	struct event :
		public wawo::ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(event)
	public:
		u8_t			id;
		WWSP<packet>	data;
		udata64			info;

		explicit event(u8_t const& id_, WWSP<packet> const& data_ = NULL, udata64 const& info_ = udata64(0) ):
			id(id_),
			data(data_),
			info(info_)
		{
		}

		~event() {
#ifdef _DEBUG
			WAWO_ASSERT( id != 0xFF ) ;
			id = 0xFF;
			data = NULL;
#endif
		}
	};
}}
#endif