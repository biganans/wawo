#ifndef _WAWO_LOG_FILE_LOGGER_H_
#define _WAWO_LOG_FILE_LOGGER_H_

#include <wawo/log/logger_abstract.hpp>

namespace wawo { namespace log {
	class file_logger: public logger_abstract {

	public:

		file_logger(wawo::len_cstr const& logFile);
		~file_logger();

		void write( LogLevelMask const& level, char const* const log, u32_t const& len ) ;

	private:
		FILE* m_fp;
	};
}}
#endif
