#include <wawo/log/FormatNormal.h>

namespace wawo { namespace log {
	
	int FormatNormal::Format( char* const buffer, u32_t const& size, char const* const format, va_list valist ) {
		return vsnprintf( buffer, size , format, valist );
	}

}}