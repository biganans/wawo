#ifndef _WAWO_LOG_CONSOLE_LOGGER_H_
#define _WAWO_LOG_CONSOLE_LOGGER_H_

#include <wawo/log/logger_abstract.hpp>

namespace wawo { namespace log {
	class console_logger: public logger_abstract {
		WAWO_DECLARE_NONCOPYABLE(console_logger)

	public:
		console_logger();
		~console_logger();

		void write( logger_abstract::LogLevelMask const& level, char const* const log, u32_t const& len ) ;
	};
}}

#endif //_WAWO_CONSOLE_LOGGER_H_
