#include <wawo/core.h>

#include <wawo/log/LoggerManager.h>
#include <wawo/time/time.hpp>
#include <wawo/log/FormatNormal.h>
#include <wawo/thread/Mutex.hpp>

#include <wawo/log/ConsoleLogger.h>
#include <wawo/log/FileLogger.h>

#ifdef WAWO_PLATFORM_GNU
	#include <wawo/log/SysLogger.h>
#endif


namespace wawo { namespace log {

	LoggerManager::LoggerManager() :
		m_consoleLogger(NULL),
		m_loggers(),
		m_defaultFormatInterface(new FormatNormal()),
		m_isInited(false),
		m_logBufferIdx(0)
	{
		Init();
	}

	LoggerManager::~LoggerManager() {
		Deinit();
	}

	void LoggerManager::Init() {

		if( m_isInited ) {
			return ;
		}

#if WAWO_USE_CONSOLE_LOGGER
		m_consoleLogger = WWRP<Logger_Abstract>( new ConsoleLogger()) ;
		m_consoleLogger->SetLevel ( WAWO_CONSOLE_LOGGER_LEVEL ) ;
		AddLogger( m_consoleLogger );
#endif

		if( m_logBufferIdx > 0 ) {
			//flush log buffer

			for( int i=0;i<m_logBufferIdx;i++ ) {
				std::vector< WWRP<Logger_Abstract> >::iterator it = m_loggers.begin();

				while( it != m_loggers.end() ) {
					(*it)->Write( m_logLevel[i], m_logBuffer[i].CStr(), m_logBuffer[i].Len() );
					++it;
				}
			}

			m_logBufferIdx = 0 ;
		}

		m_isInited = true;
	}

	void LoggerManager::Deinit() {
		//LockGuard<SharedMutex> _lg( m_mutexLoggers );
		m_loggers.clear();
	}

	void LoggerManager::AddLogger( WWRP<Logger_Abstract> const& logger) {
		//LockGuard<SharedMutex> _lg( m_mutexLoggers ) ;
		m_loggers.push_back(logger);
	}


	void LoggerManager::RemoveLogger( WWRP<Logger_Abstract> const& logger ) {
		//LockGuard<SharedMutex> _lg( m_mutexLoggers ) ;

		std::vector< WWRP<Logger_Abstract> >::iterator it = m_loggers.begin();

		while( it != m_loggers.end() ) {
			if( (*it) == logger ) {
				it = m_loggers.erase( it );
			} else {
				++it;
			}
		}
	}

	#define LOG_BUFFER_SIZE 4096
	#define TRACE_INFO_SIZE 1024

	void LoggerManager::Write( Logger_Abstract::LogLevelMask const& level, char const* const file, char const* const func, int line, ... ) {

#ifdef _DEBUG
		char __traceInfo[TRACE_INFO_SIZE] = {0};
		snprintf(__traceInfo, ((sizeof(__traceInfo)/sizeof(__traceInfo[0])) - 1), "TRACE: %s:[%d] %s", file, line, func );
#endif
		wawo::Tid tid = wawo::get_thread_id();
		char log_buffer[LOG_BUFFER_SIZE] = {0};
		int idx_tid = 0;
		int snwrite = snprintf( log_buffer + idx_tid, LOG_BUFFER_SIZE- idx_tid, "[%llu]",tid);
		WAWO_ASSERT( snwrite < (LOG_BUFFER_SIZE- idx_tid) );
		if (snwrite == -1) {
			WAWO_THROW_EXCEPTION("snprintf failed for loggerManager::Write");
		}
		idx_tid += snwrite;

		//for log buffer
		if( !m_isInited )
		{
			WWSP<FormatInterface> formatInterface;

			formatInterface = GetDefaultFormatInterface();
			va_list valist;

			va_start( valist, line );
			char* fmt;
			fmt = va_arg(valist , char*);
			int size = formatInterface->Format( log_buffer + idx_tid, LOG_BUFFER_SIZE-idx_tid, fmt, valist );
			va_end(valist);

			m_logBuffer[m_logBufferIdx] = wawo::Len_CStr(log_buffer, size ) ;

#ifdef _DEBUG
			m_logBuffer[m_logBufferIdx] += __traceInfo;
#endif
			m_logLevel[m_logBufferIdx] = level;
			m_logBufferIdx ++ ;

			return ;
		}

		WWSP<FormatInterface> formatInterface;
		std::vector< WWRP<Logger_Abstract> >::iterator it = m_loggers.begin();
		while( it != m_loggers.end() ) {
			if (!(*it)->TestLevel(level)) { ++it; continue; }
			WAWO_ASSERT( (*it) != NULL );

			int idx_fmt = idx_tid;
			formatInterface = (*it)->GetFormat() ;
			if( formatInterface == NULL ) {
				formatInterface = GetDefaultFormatInterface();
			}

			va_list valist;
			va_start(valist, line);
			char* fmt;
			fmt = va_arg(valist, char*);
			int fmtsize = formatInterface->Format( log_buffer+ idx_fmt, LOG_BUFFER_SIZE- idx_fmt, fmt, valist );
			va_end(valist);

			if(fmtsize == -1) {
				WAWO_THROW_EXCEPTION("log format failed");
			}
			WAWO_ASSERT(fmtsize != -1 );

			WAWO_ASSERT(snwrite < (LOG_BUFFER_SIZE - idx_fmt));
			if (fmtsize < (LOG_BUFFER_SIZE - idx_fmt)) {
				idx_fmt += fmtsize;
			} else {
				idx_fmt = LOG_BUFFER_SIZE;
			}

#ifdef _DEBUG
			int len = strlen(__traceInfo) + 1;

			if ((LOG_BUFFER_SIZE - idx_fmt) < len) {
				int tsnsize = snprintf((log_buffer + (LOG_BUFFER_SIZE-(len+5))), (len+5), "...\n%s", __traceInfo);
				idx_fmt = (LOG_BUFFER_SIZE-1);
				(void)&tsnsize;
			} else {
				int tsnsize = snprintf((log_buffer + idx_fmt), LOG_BUFFER_SIZE - idx_fmt, "\n%s", __traceInfo);
				idx_fmt += tsnsize;
			}
			WAWO_ASSERT(idx_fmt < LOG_BUFFER_SIZE);
			log_buffer[idx_fmt] = '\0';
#endif
			(*it++)->Write( level, log_buffer, idx_fmt);
		}
	}

	WWSP<FormatInterface>& LoggerManager::GetDefaultFormatInterface() {
		return m_defaultFormatInterface ;
	}
}}
