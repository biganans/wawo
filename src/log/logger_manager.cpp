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
		m_defaultFormatInterface(new format_normal()),
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

#if WAWO_USE_CONSOLE_LOGGER
		m_consoleLogger = wawo::make_ref <console_logger>();
		m_consoleLogger->SetLevel ( WAWO_CONSOLE_LOGGER_LEVEL ) ;
		add_logger( m_consoleLogger );
#endif

		if( m_logBufferIdx > 0 ) {
			//flush log buffer

			for( int i=0;i<m_logBufferIdx;++i ) {
				std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();

				while( it != m_loggers.end() ) {
					(*it)->write( m_logLevel[i], m_logBuffer[i].cstr, m_logBuffer[i].len );
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

	#define LOG_BUFFER_SIZE_MAX		(1024*4)
	#define TRACE_INFO_SIZE 1024

	void logger_manager::write( logger_abstract::LogLevelMask const& level, char const* const file, int line, char const* const func, ... ) {

#ifdef _DEBUG
		char __traceInfo[TRACE_INFO_SIZE] = {0};
		snprintf(__traceInfo, ((sizeof(__traceInfo)/sizeof(__traceInfo[0])) - 1), "TRACE: %s:[%d] %s", file, line, func );
#endif
		wawo::u64_t tid = wawo::this_thread::get_id();
		char log_buffer[LOG_BUFFER_SIZE_MAX] = {0};
		wawo::size_t idx_tid = 0;
		int snwrite = snprintf( log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX- idx_tid, "[%llu]",tid);
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
			int size = formatInterface->Format( log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX-idx_tid, fmt, valist );
			va_end(valist);

			m_logBuffer[m_logBufferIdx] = wawo::len_cstr(log_buffer, size ) ;

#ifdef _DEBUG
			m_logBuffer[m_logBufferIdx] += __traceInfo;
#endif
			m_logLevel[m_logBufferIdx] = level;
			m_logBufferIdx ++ ;

			(void)line;
			return ;
		}

		WWSP<format_interface> formatInterface;
		std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();
		while( it != m_loggers.end() ) {
			if (!(*it)->TestLevel(level)) { ++it; continue; }
			WAWO_ASSERT( (*it) != NULL );

			int idx_fmt = idx_tid;
			formatInterface = (*it)->GetFormat() ;
			if( formatInterface == NULL ) {
				formatInterface = get_default_format_interface();
			}

			va_list valist;
			va_start(valist, func);
			char* fmt;
			fmt = va_arg(valist, char*);
			int fmtsize = formatInterface->Format( log_buffer+ idx_fmt, LOG_BUFFER_SIZE_MAX- idx_fmt, fmt, valist );
			va_end(valist);

			if(fmtsize == -1) {
				WAWO_THROW("log format failed");
			}
			WAWO_ASSERT(fmtsize != -1 );

			WAWO_ASSERT(snwrite < (LOG_BUFFER_SIZE_MAX - idx_fmt));
			if (fmtsize < (LOG_BUFFER_SIZE_MAX - idx_fmt)) {
				idx_fmt += fmtsize;
			} else {
				idx_fmt = LOG_BUFFER_SIZE_MAX;
			}

#ifdef _DEBUG
			wawo::size_t length = wawo::strlen(__traceInfo) + 1;

			if ( (wawo::size_t)(LOG_BUFFER_SIZE_MAX - idx_fmt) < length) {
				int tsnsize = snprintf((log_buffer + (LOG_BUFFER_SIZE_MAX-(length+5))), (length+5), "...\n%s", __traceInfo);
				idx_fmt = (LOG_BUFFER_SIZE_MAX-1);
				(void)&tsnsize;
			} else {
				int tsnsize = snprintf((log_buffer + idx_fmt), LOG_BUFFER_SIZE_MAX - idx_fmt, "\n%s", __traceInfo);
				idx_fmt += tsnsize;
			}
			WAWO_ASSERT(idx_fmt < LOG_BUFFER_SIZE_MAX);
			log_buffer[idx_fmt] = '\0';
#endif
			(*it++)->write( level, log_buffer, idx_fmt);

			(void)file;
			(void)func;
		}
	}

	WWSP<format_interface>& logger_manager::get_default_format_interface() {
		return m_defaultFormatInterface ;
	}
}}
