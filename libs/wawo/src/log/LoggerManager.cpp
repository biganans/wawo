#include <wawo/core.h>

#include <wawo/log/LoggerManager.h>
#include <wawo/time/time.hpp>
#include <wawo/log/FormatNormal.h>
#include <wawo/thread/Mutex.h>

#ifdef WAWO_PLATFORM_WIN
	#include <wawo/log/FileLogger.h>
#endif

#ifdef WAWO_PLATFORM_POSIX
	#include <wawo/log/Syslogger.h>
#endif

namespace wawo { namespace log {

	LoggerManager::LoggerManager() :
		m_isInited(false),
		m_logBufferIdx(0) {

		m_defaultFormatInterface = new FormatNormal();
		Init();
	}

	LoggerManager::~LoggerManager() {
		Deinit();
		WAWO_DELETE( m_defaultFormatInterface );
	}

	void LoggerManager::Init() {

		if( m_isInited ) {
			return ;
		}

#if WAWO_USE_CONSOLE_LOGGER
		m_consoleLogger = WAWO_SHARED_PTR<Logger_Abstract>( new ConsoleLogger()) ;
		m_consoleLogger->SetLevel ( WAWO_CONSOLE_LOGGER_LEVEL ) ;
		AddLogger( m_consoleLogger );
#endif

		if( m_logBufferIdx > 0 ) {
			//flush log buffer

			for( int i=0;i<m_logBufferIdx;i++ ) {
				std::vector< WAWO_SHARED_PTR<Logger_Abstract> >::iterator it = m_loggers.begin();

				while( it != m_loggers.end() ) {
					(*it)->Write( m_logLevel[i], m_logBuffer[i].c_str(), m_logBuffer[i].length() );
					++it;
				}
			}

			m_logBufferIdx = 0 ;
		}

		m_isInited = true;
	}

	void LoggerManager::Deinit() {
		LockGuard<SharedMutex> _lg( m_mutexLoggers );
		m_loggers.clear();
	}

	void LoggerManager::AddLogger( WAWO_SHARED_PTR<Logger_Abstract> const& logger) {
		LockGuard<SharedMutex> _lg( m_mutexLoggers ) ;
		m_loggers.push_back(logger);
	}


	void LoggerManager::RemoveLogger( WAWO_SHARED_PTR<Logger_Abstract> const& logger ) {
		LockGuard<SharedMutex> _lg( m_mutexLoggers ) ;

		std::vector< WAWO_SHARED_PTR<Logger_Abstract> >::iterator it = m_loggers.begin();

		while( it != m_loggers.end() ) {

			if( (*it) == logger ) {
				it = m_loggers.erase( it );
			} else {
				++it;
			}
		}
	}

	#define LOG_BUFFER_SIZE 8192
	#define TRACE_INFO_SIZE 1024

	void LoggerManager::Write( Logger_Abstract::LogLevelMask const& level, char const* const file, char const* const func, int const& line, char const* const format, ... ) {

		SharedLockGuard<SharedMutex> _lg( m_mutexLoggers );

#ifdef _DEBUG
		char __traceInfo[TRACE_INFO_SIZE] = {0};
		snprintf(__traceInfo, ((sizeof(__traceInfo)/sizeof(__traceInfo[0])) - 1), "\nTRACE: %s:[%d] %s", file, line, func );
#endif
		//for log buffer
		if( !m_isInited )
		{
			char log_buffer[LOG_BUFFER_SIZE];

			FormatInterface* formatInterface;

			formatInterface = GetDefaultFormatInterface();
			va_list valist;

			va_start( valist, format );
			formatInterface->Format( log_buffer, sizeof(log_buffer)/sizeof(char), format, valist );
			va_end(valist);

			m_logBuffer[m_logBufferIdx] = std::string(log_buffer) ;

#ifdef _DEBUG
			m_logBuffer[m_logBufferIdx].append(__traceInfo);
#endif
			m_logLevel[m_logBufferIdx] = level;
			m_logBufferIdx ++ ;

			return ;
		}

		FormatInterface* formatInterface;
		std::vector< WAWO_SHARED_PTR<Logger_Abstract> >::iterator it = m_loggers.begin();
		while( it != m_loggers.end() ) {

			if( !(*it)->TestLevel(level) ) { return ; }

			WAWO_ASSERT( (*it) != NULL );
			char log_buffer[LOG_BUFFER_SIZE] = {0};

			formatInterface = (*it)->GetFormat() ;
			if( formatInterface == NULL ) {
				formatInterface = GetDefaultFormatInterface();
			}

			va_list valist;
			va_start( valist, format );
			int size = formatInterface->Format( log_buffer, LOG_BUFFER_SIZE, format, valist );
			va_end(valist);

#define	LIMIT_SNPRINTF_LENGTH (LOG_BUFFER_SIZE-TRACE_INFO_SIZE)
#ifdef LIMIT_SNPRINTF_LENGTH
			if( size > LIMIT_SNPRINTF_LENGTH || size == -1 ) {
				size = LIMIT_SNPRINTF_LENGTH;
			}
#endif

#ifdef _DEBUG
			//printf("-------size=%d\n", size );
			int len = strlen(__traceInfo)+1;
			//printf("-------strlen(__traceInfo)=%d\n", len );
			snprintf( (char*)(&log_buffer[size]), len, "%s", __traceInfo ) ;
			size += len;
#endif

			log_buffer[LOG_BUFFER_SIZE-1] = '\0';
			(*it++)->Write( level, log_buffer, size );
			//(it++);
		}
	}


	FormatInterface*& LoggerManager::GetDefaultFormatInterface() {
		return m_defaultFormatInterface ;
	}

}}