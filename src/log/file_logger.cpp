#include <wawo/log/file_logger.h>
#include <wawo/time/time.hpp>

#include <stdio.h>
#include <fcntl.h>

namespace wawo { namespace log {

	file_logger::file_logger(std::string const& log_file ) :
		logger_abstract(),
		m_fp(NULL)
	{
		m_fp = fopen(log_file.c_str() , "a+" );
		if( m_fp == NULL ) {
			char err_msg[1024] = {0};
			snprintf(err_msg, 1024,"fopen(%s)=%d", log_file.c_str(), wawo::get_last_errno() );
			WAWO_THROW(err_msg);
		}
	}

	file_logger::~file_logger() {
		if(m_fp) {
			fclose(m_fp);
		}
	}

	void file_logger::write( log_mask const& mask, char const* const log, wawo::size_t const& len ) {
		WAWO_ASSERT(test_mask(mask));
		fwrite( log,sizeof(char), len, m_fp );
		fflush(m_fp);
	}
}}