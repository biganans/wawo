#include <wawo/core.hpp>

#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/log/format_normal.h>
#include <wawo/mutex.hpp>

#include <wawo/log/console_logger.h>
#include <wawo/log/file_logger.h>

#ifdef WAWO_PLATFORM_GNU
	#include <wawo/log/sys_logger.h>
#endif

#include <wawo/thread.hpp>


namespace wawo { namespace log {

	logger_manager::logger_manager() :
		m_consoleLogger(NULL),
		m_loggers(),
		m_defaultFormatInterface(NULL),
		m_isInited(false),
		m_logBufferIdx(0)
	{
		init();
	}

	logger_manager::~logger_manager() {
		deinit();
	}

	void logger_manager::init() {

		if( m_isInited ) {
			return ;
		}

		m_defaultFormatInterface = wawo::make_shared<format_normal>();
		WAWO_ASSERT(m_defaultFormatInterface != NULL);

#if WAWO_USE_CONSOLE_LOGGER
		m_consoleLogger = wawo::make_ref <console_logger>();
		m_consoleLogger->set_mask_by_level( WAWO_CONSOLE_LOGGER_LEVEL ) ;
		add_logger( m_consoleLogger );
#endif

		if( m_logBufferIdx > 0 ) {
			for( int i=0;i<m_logBufferIdx;++i ) {
				std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();
				while( it != m_loggers.end() ) {
					(*it)->write( m_log_buffer_mask[i], m_log_buffer[i].c_str(), m_log_buffer[i].length() );
					++it;
				}
			}
			m_logBufferIdx = 0 ;
		}

		m_isInited = true;
	}

	void logger_manager::deinit() {
		m_loggers.clear();
	}

	void logger_manager::add_logger( WWRP<logger_abstract> const& logger) {
		m_loggers.push_back(logger);
	}

	void logger_manager::remove_logger( WWRP<logger_abstract> const& logger ) {
		std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();

		while( it != m_loggers.end() ) {
			if( (*it) == logger ) {
				it = m_loggers.erase( it );
			} else {
				++it;
			}
		}
	}
	WWSP<format_interface> const& logger_manager::get_default_format_interface() const {
		return m_defaultFormatInterface;
	}

	#define LOG_BUFFER_SIZE_MAX		(1024*4)
	#define TRACE_INFO_SIZE 1024

	void logger_manager::write( log_mask const& mask, char const* const file, int line, char const* const func, ... ) {

#ifdef _DEBUG
		char __traceInfo[TRACE_INFO_SIZE] = {0};
		snprintf(__traceInfo, ((sizeof(__traceInfo)/sizeof(__traceInfo[0])) - 1), "TRACE: %s:[%d] %s", file, line, func );
#endif
		const wawo::u64_t tid = wawo::this_thread::get_id();
		char log_buffer[LOG_BUFFER_SIZE_MAX] = {0};

		std::string local_time_str;
		wawo::time::curr_localtime_str(local_time_str);
		wawo::size_t idx_tid = 0;
		int snwrite = snprintf( log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX- idx_tid, "[%s][%c][%llu]", local_time_str.c_str(),__log_mask_char[mask], tid);
		if (snwrite == -1) {
			WAWO_THROW("snprintf failed for loggerManager::write");
		}
		WAWO_ASSERT( (wawo::size_t)snwrite < (LOG_BUFFER_SIZE_MAX - idx_tid));
		idx_tid += snwrite;

		//for log buffer
		if( !m_isInited )
		{
			WWSP<format_interface> formatInterface;

			formatInterface = get_default_format_interface();
			va_list valist;

			va_start( valist, func );
			char* fmt = va_arg(valist , char*);
			int size = formatInterface->format( log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX-idx_tid, fmt, valist );
			va_end(valist);

			m_log_buffer[m_logBufferIdx] = std::string(log_buffer, size ) ;

#ifdef _DEBUG
			m_log_buffer[m_logBufferIdx] += __traceInfo;
#endif
			m_log_buffer_mask[m_logBufferIdx] = mask;
			m_logBufferIdx ++ ;

			(void)line;
			return ;
		}

		WWSP<format_interface> formatInterface;
		std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();
		while( it != m_loggers.end() ) {
			if (!(*it)->test_mask(mask)) { ++it; continue; }
			WAWO_ASSERT( (*it) != NULL );

			int idx_fmt = idx_tid;
			formatInterface = (*it)->get_formator() ;
			if( formatInterface == NULL ) {
				formatInterface = get_default_format_interface();
			}

			va_list valist;
			va_start(valist, func);
			char* fmt;
			fmt = va_arg(valist, char*);
			int fmtsize = formatInterface->format( log_buffer+idx_fmt, LOG_BUFFER_SIZE_MAX- idx_fmt, fmt, valist );
			va_end(valist);

			WAWO_ASSERT(fmtsize != -1 );
			WAWO_ASSERT(snwrite < (LOG_BUFFER_SIZE_MAX - idx_fmt));
			idx_fmt += fmtsize;

#ifdef _DEBUG
			wawo::size_t length = wawo::strlen(__traceInfo)+5;
			if ( (wawo::size_t)(LOG_BUFFER_SIZE_MAX) < (length+idx_fmt)) {
				int tsnsize = snprintf((log_buffer + (LOG_BUFFER_SIZE_MAX-length)), (length), "...\n%s", __traceInfo);
				idx_fmt = (LOG_BUFFER_SIZE_MAX-1);
				(void)&tsnsize;
			} else {
				int tsnsize = snprintf((log_buffer + idx_fmt), LOG_BUFFER_SIZE_MAX - idx_fmt, "\n%s", __traceInfo);
				idx_fmt += tsnsize;
			}
#endif
			WAWO_ASSERT(idx_fmt < LOG_BUFFER_SIZE_MAX);
			if (idx_fmt == (LOG_BUFFER_SIZE_MAX - 1)) {
				log_buffer[LOG_BUFFER_SIZE_MAX-4] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-3] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-2] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-1] = '\n';
			} else {
				log_buffer[idx_fmt++] = '\n';
			}
			(*it++)->write( mask, log_buffer, idx_fmt);

			(void)file;
			(void)func;
		}
	}
}}
