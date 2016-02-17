#include <wawo/log/FileLogger.h>
#include <wawo/time/time.hpp>

#include <stdio.h>
#include <fcntl.h>

namespace wawo { namespace log {

	FileLogger::FileLogger(std::string const& logFile) :
		Logger_Abstract(),
		m_fp(NULL)
	{

		m_fp = fopen( logFile.c_str() , "a+" );

		if( m_fp == NULL ) {
			//fclose(m_fp);
			exit( wawo::E_INIT_LOG_FILE_FAILED );
		}

	}

	FileLogger::~FileLogger() {
		if(m_fp) {
			fclose(m_fp);
		}
	}

	void FileLogger::Write( LogLevelMask const& level, char const* const log, uint32_t const& length ) {

		if( !TestLevel( level) ) {
			 return ;
		}

		char head_buffer[128] = {0};
		char log_char = GetLogLevelChar( level );

		Len_CStr local_time_str;
		wawo::time::curr_localtime_str(local_time_str);

		sprintf( head_buffer, "[%s][%c]", local_time_str.CStr(), log_char );

		fwrite( head_buffer,sizeof(char), strlen(head_buffer), m_fp );
		fwrite( log,sizeof(char), length, m_fp );

		fputs("\n", m_fp);
		fflush(m_fp);
	}

} }


