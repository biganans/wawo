#ifndef _WAWO_LOG_FILE_LOGGER_H_
#define _WAWO_LOG_FILE_LOGGER_H_

#include <wawo/log/Logger_Abstract.hpp>
#include <string>

namespace wawo { namespace log {
	class FileLogger: public Logger_Abstract {

	public:

		FileLogger(std::string const& logFile);
		~FileLogger();

		void Write( LogLevelMask const& level, char const* const log, uint32_t const& length ) ;

	private:
		FILE* m_fp;
	};
}}
#endif