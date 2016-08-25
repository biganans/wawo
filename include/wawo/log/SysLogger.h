#ifndef _WAWO_LOG_SYSLOG_LOGGER_H_
#define _WAWO_LOG_SYSLOG_LOGGER_H_

#ifdef WAWO_PLATFORM_GNU

#define WAWO_IDENT_LENGTH 32

#include <wawo/log/Logger_Abstract.hpp>

	namespace wawo { namespace log {
		class SysLogger: public Logger_Abstract {

		public:

			SysLogger( char const* const ident );
			~SysLogger();

			/**
			 *  syslog has a 1024 length limit, so we'll split log into piece if it exceed 1024
			 */
			void Write( LogLevelMask const& level, char const* const log, u32_t const& length ) = 0 ;

		private:
			static void Syslog( int const& level, char const* const log, u32_t const& length );
			char m_ident[32] ;
		};
	}}

#endif
#endif