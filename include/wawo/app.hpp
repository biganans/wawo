#ifndef _WAWO_APP_HPP_
#define _WAWO_APP_HPP_

#include <wawo/core.hpp>
#include <wawo/signal/signal_handler_abstract.h>
#include <wawo/signal/signal_manager.h>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>

#include <wawo/net/io_event_loop.hpp>
#include <wawo/net/wcp.hpp>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace wawo {

	class app :
		public wawo::signal::signal_handler_abstract
	{

	private:
		bool m_should_exit;

		wawo::mutex m_mutex;
		wawo::condition m_cond;

	public:
		app( u8_t const& c = 0 ) :
			signal_handler_abstract(),
			m_should_exit(false)
		{
			init_log();
			init_signal();
			init_net(c);
		}

		~app()
		{
			deinit_net();
		}

		void deinit_log() {}
		void init_log() {
			WWRP<wawo::log::logger_abstract> fileLogger;
			fileLogger = wawo::make_ref<wawo::log::file_logger>("./wawo.log");
			fileLogger->SetLevel(WAWO_FILE_LOGGER_LEVEL);
			wawo::log::logger_manager::instance()->add_logger(fileLogger);
		}

		void deinit_signal() {
			wawo::unique_lock<wawo::mutex> ulk(m_mutex);

			//@todo
			//wawo::signal::signal_manager::instance()->register_signal(SIGINT, this);
			//wawo::signal::signal_manager::instance()->register_signal(SIGTERM, this);
		}
		void init_signal() {
			wawo::unique_lock<wawo::mutex> ulk(m_mutex);

#ifdef WAWO_PLATFORM_GNU
			wawo::signal::signal_manager::instance()->register_signal(SIGPIPE, NULL);
#endif
			wawo::signal::signal_manager::instance()->register_signal(SIGINT, this);
			wawo::signal::signal_manager::instance()->register_signal(SIGTERM, this);
		}

		void init_net( u8_t const& c ) {
			if (c != 0) {
				TASK_SCHEDULER->set_concurrency(c);
			}

			TASK_SCHEDULER->start();
			wawo::net::io_event_loop_group::instance()->init(WAWO_DEFAULT_WCP_OBSERVER_COUNT);
		}

		void deinit_net() {
			wawo::net::io_event_loop_group::instance()->deinit();
			TASK_SCHEDULER->stop();
		}

		void raise_signal( int signo ) {
			wawo::signal::signal_manager::instance()->raise_signal(signo);
		}

		void handle_signal( int signo ) {
			wawo::unique_lock<wawo::mutex> ulk(m_mutex);

			WAWO_INFO( "[APP]receive signo: %d", signo );
			m_cond.notify_one();

#ifdef WAWO_PLATFORM_GNU
			if( signo == SIGPIPE ) {
				return ;//IGN
			}
#endif

			switch (signo) {
			case SIGINT:
			case SIGTERM:
			{
				m_should_exit = true;
			}
			break;
			}
		}

		bool should_exit() const {
			return m_should_exit;
		}

		int run() {
			return run_until( std::chrono::system_clock::now() + std::chrono::seconds(3600*24*12*10) );
		}

		template <class _Rep, class _Period>
		int run_for(std::chrono::duration<_Rep, _Period> const& duration)
		{
			wawo::unique_lock<wawo::mutex> ulk(m_mutex);
			const std::chrono::time_point<std::chrono::system_clock> exit_tp = std::chrono::system_clock::now() + duration;
			while (!m_should_exit) {
				const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
				if (now>=exit_tp) {
					break;
				}
				m_cond.wait_for(ulk, exit_tp-now );
			}
			return 0;
		}

		template <class _Clock, class _Duration>
		int run_until(std::chrono::time_point<_Clock, _Duration> const& tp)
		{
			wawo::unique_lock<wawo::mutex> ulk(m_mutex);
			while (!m_should_exit) {
				const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
				if (now >= tp) {
					break;
				}
				m_cond.wait_until(ulk, tp);
			}
			return 0;
		}

		u32_t static get_process_id() {

#if WAWO_ISWIN
			return GetCurrentProcessId();
#elif WAWO_ISGNU
			WAWO_ASSERT( !"TODO" );
			return 12321;
#endif
		}
	};
}
#endif
