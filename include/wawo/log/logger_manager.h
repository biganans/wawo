#ifndef _WAWO_LOG_LOGGER_MANAGER_H_
#define _WAWO_LOG_LOGGER_MANAGER_H_

#include <vector>

#include <wawo/singleton.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/log/logger_abstract.hpp>

namespace wawo { namespace log {

	class logger_manager:
		public wawo::singleton<logger_manager>
	{
	public:
		logger_manager();
		virtual ~logger_manager();

		void init(); //ident for linux
		void deinit();

		void add_logger( WWRP<logger_abstract> const& logger);
		void remove_logger(WWRP<logger_abstract> const& logger);

		void write(logger_abstract::LogLevelMask const& level,char const* const file, int line, char const* const func, ... );

		WWSP<format_interface>& get_default_format_interface();

	private:
		WWRP<logger_abstract> m_consoleLogger;
		std::vector< WWRP<logger_abstract> > m_loggers;
		WWSP<format_interface> m_defaultFormatInterface;

		bool m_isInited;
		int m_logBufferIdx;

		logger_abstract::LogLevelMask m_logLevel[1024];
		wawo::len_cstr m_logBuffer[1024];
	};
}}

#define WAWO_LOG_MANAGER	( wawo::log::logger_manager::instance() )

#define WAWO_LOG_DEBUG(...)			(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	WAWO_LOG_INFO(...)			(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	WAWO_LOG_NOTICE(...)		(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_NOTICE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	WAWO_LOG_WARN(...)			(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))
#define	WAWO_LOG_ERR(...)			(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_ERR, __FILE__, __LINE__, __FUNCTION__,__VA_ARGS__ ))
#define	WAWO_LOG_CORE(...)			(WAWO_LOG_MANAGER->write( wawo::log::logger_abstract::LOG_LEVEL_CORE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__ ))

#define WAWO_DEBUG WAWO_LOG_DEBUG
#define	WAWO_INFO WAWO_LOG_INFO
#define	WAWO_NOTICE WAWO_LOG_NOTICE
#define	WAWO_WARN WAWO_LOG_WARN
#define	WAWO_ERR WAWO_LOG_ERR
#define WAWO_CORE WAWO_LOG_CORE

#ifndef _DEBUG
	#undef WAWO_DEBUG
	#define WAWO_DEBUG(...)
#endif
#endif