#ifndef _WAWO_LOG_SYSLOG_LOGGER_H_
#define _WAWO_LOG_SYSLOG_LOGGER_H_

#ifdef WAWO_PLATFORM_GNU

#define WAWO_IDENT_LENGTH 32

#include <wawo/log/logger_abstract.hpp>

	namespace wawo { namespace log {
		class sys_logger: public logger_abstract {

		public:

			sys_logger( char const* const ident );
			~sys_logger();

			/**
			 *  syslog has a 1024 len limit, so we'll split log into piece if it exceed 1024
			 */
			void write( LogLevelMask const& level, char const* const log, wawo::size_t const& len ) = 0 ;

		private:
			static void Syslog( int const& level, char const* const log, wawo::size_t const& len );
			char m_ident[32] ;
		};
	}}

#endif
#endif