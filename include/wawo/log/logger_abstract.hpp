#ifndef _WAWO_LOG_LOGGER_ABSTRACT_H_
#define _WAWO_LOG_LOGGER_ABSTRACT_H_

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/log/format_interface.h>

namespace wawo { namespace log {

	enum log_level {
		LOG_LEVEL_ERR	= 0,
		LOG_LEVEL_WARN	= 1,
		LOG_LEVEL_INFO	= 2,
		LOG_LEVEL_DEBUG = 3,
		LOG_LEVEL_MAX	= 4
	};

	enum log_mask {
		LOG_MASK_DEBUG = 0x01, //for wawo debug
		LOG_MASK_INFO = 0x02, //for generic debug
		LOG_MASK_WARN = 0x04, //warning, refuse client service
		LOG_MASK_ERR = 0x08 //fatal issue, terminate client service
	};

	const static char __log_mask_char[9] = {
		' ',
		'D',
		'I',
		' ',
		'W',
		' ',
		' ',
		' ',
		'E'
	};

	#define LOG_LEVELS_ERR		( wawo::log::LOG_MASK_ERR)
	#define LOG_LEVELS_WARN		( LOG_LEVELS_ERR | wawo::log::LOG_MASK_WARN )
	#define LOG_LEVELS_INFO		( LOG_LEVELS_WARN | wawo::log::LOG_MASK_INFO )
	#define LOG_LEVELS_DEBUG	( LOG_LEVELS_INFO | wawo::log::LOG_MASK_DEBUG )

	class logger_abstract:
		public wawo::ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(logger_abstract)

	public:
		logger_abstract():
			m_level_masks(translate_to_mask_from_level(WAWO_DEFAULT_LOG_LEVEL)),
			m_format(NULL)
		{
		}

		virtual ~logger_abstract() {}

		const inline static int translate_to_mask_from_level( u8_t const& level_int ) {
			const static int log_level_mask_config[LOG_LEVEL_MAX] = {
				LOG_LEVELS_ERR,
				LOG_LEVELS_WARN,
				LOG_LEVELS_INFO,
				LOG_LEVELS_DEBUG
			};
			if (level_int < LOG_LEVEL_MAX) {
				return log_level_mask_config[level_int];
			}
			return log_level_mask_config[WAWO_DEFAULT_LOG_LEVEL];
		}

		void add_mask( int const& mask /*level mask set*/) {
			m_level_masks |= mask;
		}

		void remove_mask( int const& masks ) {
			m_level_masks &= ~masks;
		}

		inline void set_mask_by_level( u8_t const& level ) {
			m_level_masks = translate_to_mask_from_level(level) ;
		}
		inline void reset_masks() {
			m_level_masks = translate_to_mask_from_level(WAWO_DEFAULT_LOG_LEVEL);
		}

		virtual void write( log_mask const& level, char const* const log, wawo::size_t const& len ) = 0 ;

		inline void debug( char const* const log, wawo::size_t const& len ) {
			write( LOG_MASK_DEBUG, log, len );
		}
		inline void info( char const* const log, wawo::size_t const& len ) {
			write( LOG_MASK_INFO, log, len );
		}
		inline void warn( char const* const log, wawo::size_t const& len ) {
			write( LOG_MASK_WARN, log, len );
		}
		inline void fatal( char const* const log, wawo::size_t const& len ) {
			write( LOG_MASK_ERR, log, len );
		}

		inline void set_formator( WWSP<format_interface> const& format ){
			m_format = format ;
		}
		inline WWSP<format_interface> const& get_formator() const {
			return m_format;
		}
		inline bool test_mask( log_mask const& mask) {
			return ((m_level_masks & mask ) != 0 );
		}

	private:
		int m_level_masks;
		WWSP<format_interface> m_format ;
	};
}}

#endif
