#ifndef _WAWO_LOG_FORMAT_NORMAL_H_
#define _WAWO_LOG_FORMAT_NORMAL_H_

#include <wawo/core.hpp>
#include <wawo/log/format_interface.h>

namespace wawo { namespace log {
	class format_normal: public format_interface {
	public:
		int Format( char* const buffer, u32_t const& size, char const* const format , va_list valist );
	};

}}

#endif