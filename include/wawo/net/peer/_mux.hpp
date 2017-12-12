#ifndef _WAWO_NET_PEER_HPP_
#define _WAWO_NET_PEER_HPP_

#include <atomic>
#include <wawo/core.hpp>

namespace wawo { namespace net { namespace peer {
	typedef u32_t stream_id_t;
	static std::atomic<stream_id_t> _aid(1);

	inline static stream_id_t make_stream_id() {
		return wawo::atomic_increment(&_aid);
	}

}}}
#endif