#ifndef _WAWO_LOG_FORMAT_INTERFACE_H_
#define _WAWO_LOG_FORMAT_INTERFACE_H_

namespace wawo { namespace log {

	class format_interface
	{
	public:
		virtual ~format_interface() {}
		virtual int format( char* const buffer, u32_t const& size, char const* const format , va_list valist ) = 0 ;
	};
}}
#endif 