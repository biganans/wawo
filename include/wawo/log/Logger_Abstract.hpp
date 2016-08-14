#ifndef _WAWO_LOGGER_ABSTRACT_H_
#define _WAWO_LOGGER_ABSTRACT_H_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/log/FormatInterface.h>

namespace wawo { namespace log {
	class Logger_Abstract:
		public wawo::RefObject_Abstract
	{

	public:
		struct LogLevels {
			enum LogLevelInt {
				LOG_LEVEL_CORE		= 0,
				LOG_LEVEL_FATAL		= 1,
				LOG_LEVEL_WARN		= 2,
				LOG_LEVEL_NOTICE	= 3,
				LOG_LEVEL_INFO		= 4,
				LOG_LEVEL_DEBUG		= 5,
				LOG_LEVEL_MAX		= 6
			};
		};

		enum LogLevelMask {
			LOG_LEVEL_DEBUG		= 0x01, //for wawo debug
			LOG_LEVEL_INFO		= 0x02, //for generic debug
			LOG_LEVEL_NOTICE	= 0x04, //give notice , just log
			LOG_LEVEL_WARN		= 0x08, //warning, refuse client service
			LOG_LEVEL_FATAL		= 0x10, //fatal issue, terminate client service
			LOG_LEVEL_CORE		= 0x20, //lib issue, exit gs
		};

		inline static char GetLogLevelChar( LogLevelMask const& level ) {

			switch( level ) {
			case LOG_LEVEL_DEBUG:
				{
					return 'D';
				}
				break;
			case LOG_LEVEL_INFO:
				{
					return 'I';
				}
				break;
			case LOG_LEVEL_NOTICE:
				{
					return 'N';
				}
				break;
			case LOG_LEVEL_WARN:
				{
					return 'W';
				}
				break;
			case LOG_LEVEL_FATAL:
				{
					return 'F';
				}
				break;
			case LOG_LEVEL_CORE:
				{
					return 'C';
				}
				break;
			default:
				{
					WAWO_THROW_EXCEPTION("invalid log level");
				}
				break;
			}
		}

		Logger_Abstract():
			m_level(WAWO_DEFAULT_LOG_LEVEL),
			m_format(NULL)
		{
		}

		virtual ~Logger_Abstract(){}

		#define LOG_LEVELS_CORE			( wawo::log::Logger_Abstract::LOG_LEVEL_CORE )
		#define LOG_LEVELS_FATAL		( LOG_LEVELS_CORE | wawo::log::Logger_Abstract::LOG_LEVEL_FATAL)
		#define LOG_LEVELS_WARN			( LOG_LEVELS_FATAL | wawo::log::Logger_Abstract::LOG_LEVEL_WARN )
		#define LOG_LEVELS_NOTICE		( LOG_LEVELS_WARN | wawo::log::Logger_Abstract::LOG_LEVEL_NOTICE )
		#define LOG_LEVELS_INFO			( LOG_LEVELS_NOTICE | wawo::log::Logger_Abstract::LOG_LEVEL_INFO )
		#define LOG_LEVELS_DEBUG		( LOG_LEVELS_INFO | wawo::log::Logger_Abstract::LOG_LEVEL_DEBUG )

		int TranslateToMaskLevelFromInt( int const& level_int ) {

			static int log_levels_config[LogLevels::LOG_LEVEL_MAX] = {
				LOG_LEVELS_CORE,
				LOG_LEVELS_FATAL,
				LOG_LEVELS_WARN,
				LOG_LEVELS_NOTICE,
				LOG_LEVELS_INFO,
				LOG_LEVELS_DEBUG
			};

			WAWO_ASSERT( level_int < LogLevels::LOG_LEVEL_MAX );
			return log_levels_config[level_int];
		}

		void AddLevel( int const& level /*level mask set*/) {
			if( level & Logger_Abstract::LOG_LEVEL_INFO ) {
				m_level |= Logger_Abstract::LOG_LEVEL_INFO ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_NOTICE ) {
				m_level |= Logger_Abstract::LOG_LEVEL_NOTICE ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_WARN ) {
				m_level |= Logger_Abstract::LOG_LEVEL_WARN;
			}

			if( level & Logger_Abstract::LOG_LEVEL_FATAL ) {
				m_level |= Logger_Abstract::LOG_LEVEL_FATAL ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_CORE ) {
				m_level |= Logger_Abstract::LOG_LEVEL_CORE;
			}
		}

		void RemoveLevel( int const& level ) {
			if( level & Logger_Abstract::LOG_LEVEL_INFO ) {
				m_level &= ~Logger_Abstract::LOG_LEVEL_INFO ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_NOTICE ) {
				m_level &= ~Logger_Abstract::LOG_LEVEL_NOTICE ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_WARN ) {
				m_level &= ~Logger_Abstract::LOG_LEVEL_WARN;
			}

			if( level & Logger_Abstract::LOG_LEVEL_FATAL ) {
				m_level &= ~Logger_Abstract::LOG_LEVEL_FATAL ;
			}

			if( level & Logger_Abstract::LOG_LEVEL_CORE ) {
				m_level &= ~Logger_Abstract::LOG_LEVEL_CORE;
			}
		}
		inline void SetLevel( int const& level ) {
			m_level = TranslateToMaskLevelFromInt(level) ;
		}
		inline void ResetLevel() {
			m_level = WAWO_DEFAULT_LOG_LEVEL ;
		}

		virtual void Write( LogLevelMask const& level, char const* const log, u32_t const& length ) = 0 ;

		virtual void Debug( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_DEBUG, log, length );
		}
		virtual void Info( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_INFO, log, length );
		}
		virtual void Notice( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_NOTICE, log, length );
		}
		virtual void Warn( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_WARN, log, length );
		}
		virtual void Fatal( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_FATAL, log, length );
		}
		virtual void Core( char const* const log, u32_t const& length ) {
			Write( Logger_Abstract::LOG_LEVEL_CORE, log, length );
		}
		inline void SetFormat( WWSP<FormatInterface> const& format ){
			m_format = format ;
		}
		inline WWSP<FormatInterface> const& GetFormat() {
			return m_format;
		}
		inline bool TestLevel( LogLevelMask const& level) {
			return ((m_level & level ) != 0 );
		}

	private:
		int m_level;
		WWSP<FormatInterface> m_format ;
	};
}}

#endif
