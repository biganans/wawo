#include <wawo/log/console_logger.h>
#include <wawo/time/time.hpp>

namespace wawo { namespace log {

	console_logger::console_logger() :
	logger_abstract()
	{
	}

	console_logger::~console_logger() {
	}

	void console_logger::write( logger_abstract::LogLevelMask const& level, char const* const log, wawo::size_t const& len ) {
		WAWO_ASSERT(TestLevel(level));

		len_cstr local_time_str;
		wawo::time::curr_localtime_str(local_time_str);
		printf("[%s][%c]%s \n", local_time_str.cstr, get_log_level_char(level), log );
		(void)&len;
	}

}}
