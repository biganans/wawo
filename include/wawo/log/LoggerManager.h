#ifndef _WAWO_LOG_LOGGER_MANAGER_H_
#define _WAWO_LOG_LOGGER_MANAGER_H_

#include <vector>

#include <wawo/Singleton.hpp>
#include <wawo/SmartPtr.hpp>

#include <wawo/log/Logger_Abstract.hpp>

namespace wawo { namespace log {

	class LoggerManager:
		public wawo::Singleton<LoggerManager>
	{
	public:
		LoggerManager();
		virtual ~LoggerManager();

		void Init(); //ident for linux
		void Deinit();

		void AddLogger( WWRP<Logger_Abstract> const& logger);
		void RemoveLogger(WWRP<Logger_Abstract> const& logger);

		void Write(Logger_Abstract::LogLevelMask const& level,char const* const file, char const* const func, int line, ... );

		WWSP<FormatInterface>& GetDefaultFormatInterface();

	private:
		WWRP<Logger_Abstract> m_consoleLogger;
		std::vector< WWRP<Logger_Abstract> > m_loggers;
		WWSP<FormatInterface> m_defaultFormatInterface;

		bool m_isInited;
		int m_logBufferIdx;

		Logger_Abstract::LogLevelMask m_logLevel[1024];
		wawo::Len_CStr m_logBuffer[1024];
	};
}}

#define WAWO_LOG_MANAGER	( wawo::log::LoggerManager::GetInstance() )

#define WAWO_LOG_DEBUG(...)			(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))
#define	WAWO_LOG_INFO(...)			(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))
#define	WAWO_LOG_NOTICE(...)		(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_NOTICE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))
#define	WAWO_LOG_WARN(...)			(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_WARN, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))
#define	WAWO_LOG_FATAL(...)			(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_FATAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))
#define	WAWO_LOG_CORE(...)			(WAWO_LOG_MANAGER->Write( wawo::log::Logger_Abstract::LOG_LEVEL_CORE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__ ))

#define WAWO_DEBUG WAWO_LOG_DEBUG
#define	WAWO_INFO WAWO_LOG_INFO
#define	WAWO_NOTICE WAWO_LOG_NOTICE
#define	WAWO_WARN WAWO_LOG_WARN
#define	WAWO_FATAL WAWO_LOG_FATAL
#define WAWO_CORE WAWO_LOG_CORE

#ifndef _DEBUG
	#undef WAWO_DEBUG
	#define WAWO_DEBUG(...)
#endif
#endif