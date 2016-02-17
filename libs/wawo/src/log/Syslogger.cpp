#include <wawo/core.h>

#ifdef WAWO_PLATFORM_POSIX
#include <syslog.h>
#include <wawo/log/SysLogger.h>

namespace wawo { namespace log {

	SysLogger::SysLogger( char const* const ident ) :
	Logger_Abstract() {
		strncpy( m_ident, ident, WAWO_IDENT_LENGTH-1 );
		m_ident[WAWO_IDENT_LENGTH] = '\0';

		printf("[syslog]syslog init begin\n");
		openlog( m_ident, LOG_PID, LOG_LOCAL0 );
		printf("[syslog]syslog init ok, ident name: %s", m_ident );
		syslog( LOG_INFO, "[syslog]syslog init" );
	}

	SysLogger::~SysLogger() {
		closelog();
	}

	void SysLogger::Write( LogLevelMask const& level, char const* const log, uint32_t const& length ) {

		if( !TestLevel( level) ) {
			 return ;
		}

		char head_buffer[4] = {0};
		char log_char = GetLogLevelChar( level );
		sprintf( head_buffer, "[%c]", log_char );

		int total_len = 3 + length ;
		WAWO_ASSERT( total_len <= 10240 );
		char _log[10240]; //total bufferd

		strncpy( _log, head_buffer, 3 );
		strncpy( _log + 3, log, length );
		_log[total_len] = '\0';

		switch( level ) {
			case Logger_Abstract::LOG_LEVEL_DEBUG:
				{
					SysLogger::Syslog( LOG_DEBUG,  _log, total_len );
				}
				break;
			case Logger_Abstract::LOG_LEVEL_INFO:
				{
					SysLogger::Syslog( LOG_INFO, _log, total_len );
				}
				break;
			case Logger_Abstract::LOG_LEVEL_NOTICE:
				{
					SysLogger::Syslog( LOG_NOTICE,  _log, total_len );
				}
				break;
			case Logger_Abstract::LOG_LEVEL_WARN:
				{
					SysLogger::Syslog( LOG_WARNING,  _log, total_len ) ;
				}
				break;
			case Logger_Abstract::LOG_LEVEL_FATAL:
				{
					SysLogger::Syslog( LOG_ERR,  _log, total_len );
				}
				break;
			case Logger_Abstract::LOG_LEVEL_CORE:
				{
					SysLogger::Syslog( LOG_CRIT,  _log, total_len );
				}
				break;
			default:
				{
					char const* unknown_log_str = "unknown log level found";
					SysLogger::Syslog( LOG_DEBUG, unknown_log_str, strlen(unknown_log_str) );
					SysLogger::Syslog( LOG_DEBUG, _log, total_len );
				}
				break;
			}
		}

	void SysLogger::Syslog(int const& level, char const* const log, uint32_t const& length ) {

//		printf("syslog: %s\n", log.c_str() );
#if WAWO_LOG_SPLIT_SYSLOG

		if( length > WAWO_LOG_SYSLOG_SPLIT_LENGTH ) {

			int _idx = 0;
			bool _try ;

			syslog( level, "%s", "-----SYSLOG----SPLIT-----BEGIN-----" );
			do {
				char split[WAWO_LOG_SYSLOG_SPLIT_LENGTH];
				int left_len = ( length-_idx*WAWO_LOG_SYSLOG_SPLIT_LENGTH );

				if( left_len <= 0 ) { break;}

				int to_c_len = (left_len >= WAWO_LOG_SYSLOG_SPLIT_LENGTH)? WAWO_LOG_SYSLOG_SPLIT_LENGTH : left_len;
				strncpy(split,log + (_idx*WAWO_LOG_SYSLOG_SPLIT_LENGTH), to_c_len );

				split[to_c_len] = '\0';

				syslog( level,"<-%d-> %s" ,_idx, split );
				++_idx;

				_try = (to_c_len == WAWO_LOG_SYSLOG_SPLIT_LENGTH) ;
			} while( _try ) ;

			syslog( level, "%s", "-----SYSLOG----SPLIT-----END-----" );
		} else {

//            log.append("\n");
			/*
		    char* ptr = new char[10240];
		    memset(ptr,0,10240);
		    strncpy(ptr, log.c_str(),10240 );
			*/

			syslog( level,"%s", log );
		}
#else
		syslog( level, "%s", log );
#endif
	}

}}
#endif //WAWO_PLATFORM_POSIX
