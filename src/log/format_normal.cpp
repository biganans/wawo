#include <cstdarg>
#include <wawo/log/format_normal.h>

namespace wawo { namespace log {
	
	int format_normal::Format( char* const buffer, u32_t const& size, char const* const format, va_list valist ) {
		return vsnprintf( buffer, size , format, valist );
	}

}}