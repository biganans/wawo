#include <wawo/log/console_logger.h>
#include <wawo/time/time.hpp>

namespace wawo { namespace log {

	console_logger::console_logger() :
	logger_abstract()
	{
	}

	console_logger::~console_logger() {
	}

	void console_logger::write( log_mask const& mask, char const* const log, wawo::size_t const& len ) {
		WAWO_ASSERT(test_mask(mask));
		printf("%s", log );
		(void)&len;
	}
}}