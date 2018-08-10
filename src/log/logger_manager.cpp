#include <wawo/core.hpp>

#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/log/format_normal.h>
#include <wawo/mutex.hpp>

#include <wawo/log/console_logger.h>
#include <wawo/log/file_logger.h>

#ifdef WAWO_PLATFORM_GNU
	#include <wawo/log/sys_logger.h>
#endif

namespace wawo { namespace log {

	logger_manager::logger_manager() :
		m_consoleLogger(NULL),
		m_loggers(),
		m_defaultFormatInterface(NULL),
		m_isInited(false)
	{
		init();
	}

	logger_manager::~logger_manager() {
		deinit();
	}

	void logger_manager::init() {

		if( m_isInited ) {
			return ;
		}

		m_defaultFormatInterface = wawo::make_shared<format_normal>();
		WAWO_ASSERT(m_defaultFormatInterface != NULL);

#if WAWO_USE_CONSOLE_LOGGER
		m_consoleLogger = wawo::make_ref <console_logger>();
		m_consoleLogger->set_mask_by_level( WAWO_CONSOLE_LOGGER_LEVEL ) ;
		add_logger( m_consoleLogger );
#endif
		m_isInited = true;
	}

	void logger_manager::deinit() {
		m_loggers.clear();
	}

	void logger_manager::add_logger(WWRP<logger_abstract> const& logger) {
		m_loggers.push_back(logger);
	}

	void logger_manager::remove_logger(WWRP<logger_abstract> const& logger) {
		std::vector< WWRP<logger_abstract> >::iterator it = m_loggers.begin();

		while (it != m_loggers.end()) {
			if ((*it) == logger) {
				it = m_loggers.erase(it);
			} else {
				++it;
			}
		}
	}
	WWSP<format_interface> const& logger_manager::get_default_format_interface() const {
		return m_defaultFormatInterface;
	}

#define LOG_BUFFER_SIZE_MAX		(1024*4)
#define TRACE_INFO_SIZE 1024

	void logger_manager::write(log_mask const& mask, char const* const file, int line, char const* const func, ...) {

#ifdef _DEBUG
		char __traceInfo[TRACE_INFO_SIZE] = { 0 };
		snprintf(__traceInfo, ((sizeof(__traceInfo) / sizeof(__traceInfo[0])) - 1), "TRACE: %s:[%d] %s", file, line, func);
#endif
		(void)file;
		(void)line;
		WAWO_ASSERT(m_isInited);
		const wawo::u64_t tid = wawo::this_thread::get_id();
		
		const ::size_t lc = m_loggers.size();
		for(::size_t i=0;i<lc;++i) {
			if (!m_loggers[i]->test_mask(mask)) { continue; }
			WAWO_ASSERT(m_loggers[i] != NULL);

			char log_buffer[LOG_BUFFER_SIZE_MAX] = { 0 };
			std::string local_time_str;
			wawo::time::curr_localtime_str(local_time_str);
			int idx_tid = 0;
			int snwrite = snprintf(log_buffer + idx_tid, LOG_BUFFER_SIZE_MAX - idx_tid, "[%s][%c][%llu]", local_time_str.c_str(), __log_mask_char[mask], tid);
			if (snwrite == -1) {
				WAWO_THROW("snprintf failed for loggerManager::write");
			}
			WAWO_ASSERT(snwrite < (LOG_BUFFER_SIZE_MAX - idx_tid));
			idx_tid += snwrite;

			int idx_fmt = idx_tid;
			WWSP<format_interface> formatInterface = m_loggers[i]->get_formator();
			if (formatInterface == NULL) {
				formatInterface = get_default_format_interface();
			}

			va_list valist;
			va_start(valist, func);
			char* fmt;
			fmt = va_arg(valist, char*);
			int fmtsize = formatInterface->format(log_buffer + idx_fmt, LOG_BUFFER_SIZE_MAX - idx_fmt, fmt, valist);
			va_end(valist);

			WAWO_ASSERT(fmtsize != -1);
			//@refer to: https://linux.die.net/man/3/vsnprintf
			if (fmtsize >= (LOG_BUFFER_SIZE_MAX-idx_fmt)) {
				//truncated
				fmtsize = (LOG_BUFFER_SIZE_MAX - idx_fmt) - 1;
			} else {
				idx_fmt += fmtsize;
			}
			WAWO_ASSERT(idx_fmt < LOG_BUFFER_SIZE_MAX);

#ifdef _DEBUG
			wawo::size_t length = wawo::strlen(__traceInfo)+5;
			if ( (wawo::size_t)(LOG_BUFFER_SIZE_MAX) < (length+idx_fmt)) {
				int tsnsize = snprintf((log_buffer + (LOG_BUFFER_SIZE_MAX-length)), (length), "...\n%s", __traceInfo);
				idx_fmt = (LOG_BUFFER_SIZE_MAX-1);
				(void)&tsnsize;
			} else {
				int tsnsize = snprintf((log_buffer + idx_fmt), LOG_BUFFER_SIZE_MAX - idx_fmt, "\n%s", __traceInfo);
				idx_fmt += tsnsize;
			}
#endif
			if (idx_fmt == (LOG_BUFFER_SIZE_MAX - 1)) {
				log_buffer[LOG_BUFFER_SIZE_MAX-4] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-3] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-2] = '.';
				log_buffer[LOG_BUFFER_SIZE_MAX-1] = '\n';
			} else {
				log_buffer[idx_fmt++] = '\n';
			}
			m_loggers[i]->write( mask, log_buffer, idx_fmt);

			(void)file;
			(void)func;
		}
	}
}}
