#ifndef _WAWO_NET_IO_EVENT_HPP
#define _WAWO_NET_IO_EVENT_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

#include <functional>

namespace wawo { namespace net{
	typedef std::function<void(WWRP<ref_base> const& fnctx)> fn_io_event;
	typedef std::function<void(int const& code,WWRP<ref_base> const& fnctx)> fn_io_event_error;
}}

#endif