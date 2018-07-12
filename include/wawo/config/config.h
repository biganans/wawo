#ifndef _WAWO_CONFIG_CONFIG_H_
#define _WAWO_CONFIG_CONFIG_H_


//split SYSLOG message into piece that less than 1024
#ifndef WAWO_SPLIT_SYSLOG
	#define WAWO_SPLIT_SYSLOG 1
#endif

#ifndef WAWO_SYSLOG_SPLIT_LENGTH
	#define WAWO_SYSLOG_SPLIT_LENGTH 1024
#endif

//log level

/** PLASE REFER TO loggerManager::LogLevels::LogLevel  */
#ifndef WAWO_DEFAULT_LOG_LEVEL
	#define WAWO_DEFAULT_LOG_LEVEL 3
#endif

#ifndef WAWO_USE_FILE_LOGGER
	#ifdef WAWO_PLATFORM_WIN
		#define WAWO_USE_FILE_LOGGER	1
	#else
		#define WAWO_USE_FILE_LOGGER	0
	#endif
#endif

#ifndef WAWO_USE_SYS_LOGGER
	#ifdef WAWO_PLATFORM_GNU
		#define WAWO_USE_SYS_LOGGER 1
	#else
		#define WAWO_USE_SYS_LOGGER 0
	#endif
#endif

#define WAWO_USE_CONSOLE_LOGGER 1

#ifdef _DEBUG
	#define __DEFAULT_CONSOLE_LOG_LEVEL 5
	#define __DEFAULT_FILE_LOG_LEVEL 5
	#define __DEFAULT_SYS_LOG_LEVEL 5
#else
	#define __DEFAULT_CONSOLE_LOG_LEVEL 4
	#define __DEFAULT_SYS_LOG_LEVEL 4
	#define __DEFAULT_FILE_LOG_LEVEL 4
#endif


#ifndef	WAWO_FILE_LOGGER_LEVEL
	#define WAWO_FILE_LOGGER_LEVEL	__DEFAULT_FILE_LOG_LEVEL
#endif

#ifndef WAWO_SYS_LOGGER_LEVEL
	#define WAWO_SYS_LOGGER_LEVEL	__DEFAULT_SYS_LOG_LEVEL
#endif

#ifndef WAWO_CONSOLE_LOGGER_LEVEL
	#define WAWO_CONSOLE_LOGGER_LEVEL	__DEFAULT_CONSOLE_LOG_LEVEL
#endif

#define WAWO_ENABLE_IOCP
//FOR IO MODE
#if defined(WAWO_PLATFORM_WIN) && defined(WAWO_ENABLE_IOCP)
	#define WAWO_IO_MODE_IOCP
#elif defined(WAWO_PLATFORM_GNU)
	#define WAWO_IO_MODE_EPOLL
#else
	#define WAWO_IO_MODE_SELECT
#endif

// for epoll using
#ifdef WAWO_IO_MODE_EPOLL
	#define WAWO_EPOLL_CREATE_HINT_SIZE			(10240*16)	///< max size of epoll control
	#define WAWO_EPOLL_PER_HANDLE_SIZE			(4096)	///< max size of per epoll_wait
	#define WAWO_IO_MODE_EPOLL_USE_ET
#endif


#define WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT 256

//#define WAWO_ENABLE_TASK_TRACK
#ifdef WAWO_ENABLE_TRACK_TASK
	#define WAWO_TRACE_TASK WAWO_DEBUG
#else
	#define WAWO_TRACE_TASK(...)
#endif

#define WAWO_DEFAULT_KEEPALIVE_IDLETIME	60
#define WAWO_DEFAULT_KEEPALIVE_INTERVAL	15
#define WAWO_DEFAULT_KEEPALIVE_PROBES	10

//default on
//#define WAWO_ENABLE_WCP
#ifdef WAWO_ENABLE_WCP
	#define WAWO_DEFAULT_WCP_OBSERVER_COUNT 1
#else
	#define WAWO_DEFAULT_WCP_OBSERVER_COUNT 0
#endif

//#define ENABLE_WEBSOCKET


//#define ENABLE_TRACE_IOE
#ifdef ENABLE_TRACE_IOE
	#define TRACE_IOE WAWO_INFO
#else
	#define TRACE_IOE(...)
#endif


#endif // end for _CONFIG_WAWO_CONFIG_H_