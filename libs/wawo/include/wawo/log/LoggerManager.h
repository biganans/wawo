#ifndef _WAWO_LOG_LOGGER_MANAGER_H_
#define _WAWO_LOG_LOGGER_MANAGER_H_

#include <vector>


#include <wawo/SafeSingleton.hpp>
#include <wawo/log/Logger_Abstract.hpp>
#include <wawo/log/ConsoleLogger.h>
#include <wawo/log/FileLogger.h>
#include <wawo/log/SysLogger.h>

#include <wawo/thread/Mutex.h>

namespace wawo { namespace log {

	using namespace wawo::thread;

	class LoggerManager: 
		public wawo::SafeSingleton<LoggerManager> 
	{

	public:
		LoggerManager();
		virtual ~LoggerManager();


		void Init(); //ident for linux
		void Deinit();

		void AddLogger( Logger_Abstract * const logger);
		void RemoveLogger( Logger_Abstract * const logger);


		//void SetLevel( unsigned int level );
		//void AddLevel( unsigned int level );
		//void RemoveLevel( unsigned int level);

		void Write(Logger_Abstract::LogLevelMask const& level, char const* const file, char const* const func, int const& line, char const* const format, ...);

		FormatInterface*& GetDefaultFormatInterface();

	private:

		SharedMutex m_mutexLoggers;

		Logger_Abstract* m_consoleLogger;
		std::vector<Logger_Abstract*> m_loggers;
		FormatInterface* m_defaultFormatInterface;

		bool m_isInited;
		int m_logBufferIdx;
		Logger_Abstract::LogLevelMask m_logLevel[1024];

		std::string m_logBuffer[1024];
	};
}}


#define WAWO_LOG_INSTANCE	( wawo::log::LoggerManager::GetInstance() )

#if defined(OS_IPHONE) || defined(OS_LINUX)
	#define WAWO_LOG_DEBUG(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_DEBUG,__FILE__, __FUNCTION__, __LINE__, "[" tagName "]" format, ## __VA_ARGS__ ))
	#define WAWO_LOG_INFO(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_INFO,__FILE__, __FUNCTION__, __LINE__, "[" tagName"]" format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_NOTICE(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_NOTICE,__FILE__, __FUNCTION__, __LINE__, "[" tagName"]" format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_WARN(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_WARN,__FILE__, __FUNCTION__, __LINE__, "[" tagName"]" format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_FATAL(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_FATAL,__FILE__, __FUNCTION__, __LINE__, "[" tagName"]" format, ## __VA_ARGS__ ) )
	#define WAWO_LOG_CORE(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_CORE,__FILE__, __FUNCTION__, __LINE__, "[" tagName"]" format, ## __VA_ARGS__ ) )
#else
	#define WAWO_LOG_DEBUG(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_DEBUG,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format,  ## __VA_ARGS__ ))
	#define	WAWO_LOG_INFO(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_INFO,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_NOTICE(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_NOTICE,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_WARN(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_WARN,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_FATAL(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_FATAL,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format, ## __VA_ARGS__ ) )
	#define	WAWO_LOG_CORE(tagName,format,...)				(WAWO_LOG_INSTANCE->Write( wawo::log::Logger_Abstract::LOG_LEVEL_CORE,__FILE__, __FUNCTION__, __LINE__, "[" ##tagName "]" ##format, ## __VA_ARGS__ ) )
#endif

#ifndef _DEBUG
	#undef WAWO_LOG_DEBUG
	#undef WAWO_LOG_INFO

	#define WAWO_LOG_DEBUG(...)
	#define WAWO_LOG_INFO(...)
#endif



#endif
