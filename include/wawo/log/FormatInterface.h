#ifndef _WAWO_LOG_FORMAT_INTERFACE_H_
#define _WAWO_LOG_FORMAT_INTERFACE_H_

#include <stdarg.h>

namespace wawo { namespace log {

	class FormatInterface
	{
	public:
		virtual ~FormatInterface() {}
		virtual int Format( char* const buffer, u32_t const& size, char const* const format , va_list valist ) = 0 ;
	};
}}
#endif 