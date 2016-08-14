#include <wawo/log/ConsoleLogger.h>
#include <wawo/time/time.hpp>

namespace wawo { namespace log {

	ConsoleLogger::ConsoleLogger() :
	Logger_Abstract()
	{
	}

	ConsoleLogger::~ConsoleLogger() {
	}

	void ConsoleLogger::Write( Logger_Abstract::LogLevelMask const& level, char const* const log, u32_t const& length ) {
		WAWO_ASSERT(TestLevel(level));

		Len_CStr local_time_str;
		wawo::time::curr_localtime_str(local_time_str);
		printf("[%s][%c]%s \n", local_time_str.CStr(), GetLogLevelChar(level), log );
		(void)&length;
	}

}}
