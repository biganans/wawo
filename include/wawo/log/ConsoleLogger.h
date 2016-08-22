#ifndef _WAWO_LOG_CONSOLE_LOGGER_H_
#define _WAWO_LOG_CONSOLE_LOGGER_H_

#include <wawo/log/Logger_Abstract.hpp>

namespace wawo { namespace log {
	class ConsoleLogger: public Logger_Abstract {

	public:
		ConsoleLogger();
		~ConsoleLogger();

		void Write( Logger_Abstract::LogLevelMask const& level, char const* const log, u32_t const& length ) ;
	};
}}

#endif //_WAWO_CONSOLE_LOGGER_H_
