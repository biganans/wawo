#ifndef _WAWO_NET_IO_EVENT_HPP
#define _WAWO_NET_IO_EVENT_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

namespace wawo { namespace net{
	typedef std::function<void()> fn_io_event;
	typedef std::function<void(int const& code)> fn_io_event_error;
}}

#endif