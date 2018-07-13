#include <wawo/log/file_logger.h>
#include <wawo/time/time.hpp>

#include <stdio.h>
#include <fcntl.h>

namespace wawo { namespace log {

	file_logger::file_logger(wawo::len_cstr const& logFile) :
		logger_abstract(),
		m_fp(NULL)
	{
		m_fp = fopen( logFile.cstr , "a+" );
		if( m_fp == NULL ) {
			char err_msg[1024] = {0};
			snprintf(err_msg, 1024,"fopen(%s)=%d", logFile.cstr, wawo::get_last_errno() );
			WAWO_THROW(err_msg);
		}
	}

	file_logger::~file_logger() {
		if(m_fp) {
			fclose(m_fp);
		}
	}

	void file_logger::write( LogLevelMask const& level, char const* const log, wawo::size_t const& len ) {

		WAWO_ASSERT(TestLevel(level));

		char head_buffer[128] = {0};
		char log_char = get_log_level_char( level );

		len_cstr local_time_str;
		wawo::time::curr_localtime_str(local_time_str);

		sprintf( head_buffer, "[%s][%c]", local_time_str.cstr, log_char );

		fwrite( head_buffer,sizeof(char), strlen(head_buffer), m_fp );
		fwrite( log,sizeof(char), len, m_fp );

		fputs("\n", m_fp);
		fflush(m_fp);
	}

}}
