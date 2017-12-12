#include <wawo/core.hpp>

#ifdef WAWO_PLATFORM_GNU
#include <syslog.h>
#include <wawo/log/sys_logger.h>

namespace wawo { namespace log {

	sys_logger::sys_logger( char const* const ident ) :
	logger_abstract() {
		strncpy( m_ident, ident, WAWO_IDENT_LENGTH-1 );
		m_ident[WAWO_IDENT_LENGTH] = '\0';

		printf("[syslog]syslog init begin\n");
		openlog( m_ident, LOG_PID, LOG_LOCAL0 );
		printf("[syslog]syslog init ok, ident name: %s", m_ident );
		syslog( LOG_INFO, "[syslog]syslog init" );
	}

	sys_logger::~sys_logger() {
		closelog();
	}

	void sys_logger::write( LogLevelMask const& level, char const* const log, u32_t const& len ) {

		if( !TestLevel( level) ) {
			 return ;
		}

		char head_buffer[4] = {0};
		char log_char = get_log_level_char( level );
		sprintf( head_buffer, "[%c]", log_char );

		int total_length = 3 + len ;
		WAWO_ASSERT( total_length <= 10240 );
		char _log[10240]; //total bufferd

		strncpy( _log, head_buffer, 3 );
		strncpy( _log + 3, log, len );
		_log[total_length] = '\0';

		switch( level ) {
			case logger_abstract::LOG_LEVEL_DEBUG:
				{
					sys_logger::Syslog( LOG_DEBUG,  _log, total_length );
				}
				break;
			case logger_abstract::LOG_LEVEL_INFO:
				{
					sys_logger::Syslog( LOG_INFO, _log, total_length );
				}
				break;
			case logger_abstract::LOG_LEVEL_NOTICE:
				{
					sys_logger::Syslog( LOG_NOTICE,  _log, total_length );
				}
				break;
			case logger_abstract::LOG_LEVEL_WARN:
				{
					sys_logger::Syslog( LOG_WARNING,  _log, total_length ) ;
				}
				break;
			case logger_abstract::LOG_LEVEL_ERR:
				{
					sys_logger::Syslog( LOG_ERR,  _log, total_length );
				}
				break;
			case logger_abstract::LOG_LEVEL_CORE:
				{
					sys_logger::Syslog( LOG_CRIT,  _log, total_length );
				}
				break;
			default:
				{
					char const* unknown_log_str = "unknown log level found";
					sys_logger::Syslog( LOG_DEBUG, unknown_log_str, strlen(unknown_log_str) );
					sys_logger::Syslog( LOG_DEBUG, _log, total_length );
				}
				break;
			}
		}

	void sys_logger::Syslog(int const& level, char const* const log, u32_t const& len ) {

//		printf("syslog: %s\n", log.c_str() );
#if WAWO_SPLIT_SYSLOG
		if( len > WAWO_SYSLOG_SPLIT_LENGTH ) {

			int _idx = 0;
			bool _try ;

			syslog( level, "%s", "-----SYSLOG----SPLIT-----BEGIN-----" );
			do {
				char split[WAWO_SYSLOG_SPLIT_LENGTH];
				int left_length = ( len-_idx*WAWO_SYSLOG_SPLIT_LENGTH );

				if( left_length <= 0 ) { break;}

				int to_c_length = (left_length >= WAWO_SYSLOG_SPLIT_LENGTH)? WAWO_SYSLOG_SPLIT_LENGTH : left_length;
				strncpy(split,log + (_idx*WAWO_SYSLOG_SPLIT_LENGTH), to_c_length );

				split[to_c_length] = '\0';

				syslog( level,"<-%d-> %s" ,_idx, split );
				++_idx;

				_try = (to_c_length == WAWO_SYSLOG_SPLIT_LENGTH) ;
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
#endif //WAWO_PLATFORM_GNU
