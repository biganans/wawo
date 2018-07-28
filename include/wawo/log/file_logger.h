#ifndef _WAWO_LOG_FILE_LOGGER_H_
#define _WAWO_LOG_FILE_LOGGER_H_

#include <wawo/log/logger_abstract.hpp>

namespace wawo { namespace log {
	class file_logger: public logger_abstract {

	public:

		file_logger(std::string const& log_file );
		~file_logger();

		void write( log_mask const& mask, char const* const log, wawo::size_t const& len ) ;

	private:
		FILE* m_fp;
	};
}}
#endif
