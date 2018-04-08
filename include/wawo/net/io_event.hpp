#ifndef _WAWO_NET_IO_EVENT_HPP
#define _WAWO_NET_IO_EVENT_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

namespace wawo { namespace net{
	typedef void(*fn_io_event) (WWRP<ref_base> const& cookie);
	typedef void(*fn_io_event_error) (int const& code, WWRP<ref_base> const& cookie);
}}

#endif