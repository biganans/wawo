#ifndef _WAWO_NET_CHANNEL_FUTURE_HPP
#define _WAWO_NET_CHANNEL_FUTURE_HPP

#include <wawo/future.hpp>

namespace wawo { namespace net {
	typedef future<int> channel_future;
	typedef promise<int> channel_promise;
}}

#endif