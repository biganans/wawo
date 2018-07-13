#ifndef _WAWO_LOG_LOGGER_ABSTRACT_H_
#define _WAWO_LOG_LOGGER_ABSTRACT_H_

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/log/format_interface.h>

namespace wawo { namespace log {
	class logger_abstract:
		public wawo::ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(logger_abstract)

	public:
		struct LogLevels {
			enum LogLevelInt {
				LOG_LEVEL_CORE		= 0,
				LOG_LEVEL_ERR		= 1,
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
			LOG_LEVEL_ERR		= 0x10, //fatal issue, terminate client service
			LOG_LEVEL_CORE		= 0x20, //lib issue, exit gs
		};

		inline static char get_log_level_char( LogLevelMask const& level ) {

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
			case LOG_LEVEL_ERR:
				{
					return 'E';
				}
				break;
			case LOG_LEVEL_CORE:
				{
					return 'C';
				}
				break;
			default:
				{
					WAWO_THROW("invalid log level");
				}
				break;
			}
		}

		logger_abstract():
			m_level(WAWO_DEFAULT_LOG_LEVEL),
			m_format(NULL)
		{
		}

		virtual ~logger_abstract(){}

		#define LOG_LEVELS_CORE			( wawo::log::logger_abstract::LOG_LEVEL_CORE )
		#define LOG_LEVELS_ERR		( LOG_LEVELS_CORE | wawo::log::logger_abstract::LOG_LEVEL_ERR)
		#define LOG_LEVELS_WARN			( LOG_LEVELS_ERR | wawo::log::logger_abstract::LOG_LEVEL_WARN )
		#define LOG_LEVELS_NOTICE		( LOG_LEVELS_WARN | wawo::log::logger_abstract::LOG_LEVEL_NOTICE )
		#define LOG_LEVELS_INFO			( LOG_LEVELS_NOTICE | wawo::log::logger_abstract::LOG_LEVEL_INFO )
		#define LOG_LEVELS_DEBUG		( LOG_LEVELS_INFO | wawo::log::logger_abstract::LOG_LEVEL_DEBUG )

		int TranslateToMaskLevelFromInt( int const& level_int ) {

			static int log_levels_config[LogLevels::LOG_LEVEL_MAX] = {
				LOG_LEVELS_CORE,
				LOG_LEVELS_ERR,
				LOG_LEVELS_WARN,
				LOG_LEVELS_NOTICE,
				LOG_LEVELS_INFO,
				LOG_LEVELS_DEBUG
			};

			WAWO_ASSERT( level_int < LogLevels::LOG_LEVEL_MAX );
			return log_levels_config[level_int];
		}

		void AddLevel( int const& level /*level mask set*/) {
			if( level & logger_abstract::LOG_LEVEL_INFO ) {
				m_level |= logger_abstract::LOG_LEVEL_INFO ;
			}

			if( level & logger_abstract::LOG_LEVEL_NOTICE ) {
				m_level |= logger_abstract::LOG_LEVEL_NOTICE ;
			}

			if( level & logger_abstract::LOG_LEVEL_WARN ) {
				m_level |= logger_abstract::LOG_LEVEL_WARN;
			}

			if( level & logger_abstract::LOG_LEVEL_ERR ) {
				m_level |= logger_abstract::LOG_LEVEL_ERR ;
			}

			if( level & logger_abstract::LOG_LEVEL_CORE ) {
				m_level |= logger_abstract::LOG_LEVEL_CORE;
			}
		}

		void RemoveLevel( int const& level ) {
			if( level & logger_abstract::LOG_LEVEL_INFO ) {
				m_level &= ~logger_abstract::LOG_LEVEL_INFO ;
			}

			if( level & logger_abstract::LOG_LEVEL_NOTICE ) {
				m_level &= ~logger_abstract::LOG_LEVEL_NOTICE ;
			}

			if( level & logger_abstract::LOG_LEVEL_WARN ) {
				m_level &= ~logger_abstract::LOG_LEVEL_WARN;
			}

			if( level & logger_abstract::LOG_LEVEL_ERR ) {
				m_level &= ~logger_abstract::LOG_LEVEL_ERR ;
			}

			if( level & logger_abstract::LOG_LEVEL_CORE ) {
				m_level &= ~logger_abstract::LOG_LEVEL_CORE;
			}
		}
		inline void SetLevel( int const& level ) {
			m_level = TranslateToMaskLevelFromInt(level) ;
		}
		inline void ResetLevel() {
			m_level = WAWO_DEFAULT_LOG_LEVEL ;
		}

		virtual void write( LogLevelMask const& level, char const* const log, wawo::size_t const& len ) = 0 ;

		virtual void Debug( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_DEBUG, log, len );
		}
		virtual void Info( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_INFO, log, len );
		}
		virtual void Notice( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_NOTICE, log, len );
		}
		virtual void Warn( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_WARN, log, len );
		}
		virtual void Fatal( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_ERR, log, len );
		}
		virtual void Core( char const* const log, wawo::size_t const& len ) {
			write( logger_abstract::LOG_LEVEL_CORE, log, len );
		}
		inline void SetFormat( WWSP<format_interface> const& format ){
			m_format = format ;
		}
		inline WWSP<format_interface> const& GetFormat() {
			return m_format;
		}
		inline bool TestLevel( LogLevelMask const& level) {
			return ((m_level & level ) != 0 );
		}

	private:
		int m_level;
		WWSP<format_interface> m_format ;
	};
}}

#endif
