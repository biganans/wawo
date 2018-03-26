#ifndef _WAWO_APP_HPP_
#define _WAWO_APP_HPP_

#include <wawo/core.hpp>
#include <wawo/signal/signal_handler_abstract.h>
#include <wawo/signal/signal_manager.h>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>

#include <wawo/net/socket_observer.hpp>
#include <wawo/net/wcp.hpp>

namespace wawo {

	class app :
		public wawo::signal::signal_handler_abstract
	{

	private:
		bool m_should_exit;

		wawo::thread::mutex m_mutex;
		wawo::thread::condition m_cond;

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
			wawo::thread::unique_lock<wawo::thread::mutex> ulk(m_mutex);

			//@todo
			//wawo::signal::signal_manager::instance()->register_signal(SIGINT, this);
			//wawo::signal::signal_manager::instance()->register_signal(SIGTERM, this);
		}
		void init_signal() {
			wawo::thread::unique_lock<wawo::thread::mutex> ulk(m_mutex);

#ifdef WAWO_PLATFORM_GNU
			wawo::signal::signal_manager::instance()->register_signal(SIGPIPE, NULL);
#endif
			wawo::signal::signal_manager::instance()->register_signal(SIGINT, this);
			wawo::signal::signal_manager::instance()->register_signal(SIGTERM, this);
		}

		void init_net( u8_t const& c ) {
			if (c != 0) {
				WAWO_SCHEDULER->set_concurrency(c);
			}
			WAWO_SCHEDULER->start();

#ifdef WAWO_ENABLE_WCP
			wawo::net::wcp::instance()->start();
#endif
			wawo::net::observers::instance()->init();
		}

		void deinit_net() {
#ifdef WAWO_ENABLE_WCP
			wawo::net::wcp::instance()->stop();
#endif
			wawo::net::observers::instance()->deinit();
			WAWO_SCHEDULER->stop();
		}

		void raise_signal( int signo ) {
			wawo::signal::signal_manager::instance()->raise_signal(signo);
		}

		void handle_signal( int signo ) {
			wawo::thread::unique_lock<wawo::thread::mutex> ulk(m_mutex);

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

		//ten years, in mill
		int run_for( u64_t const& time = 315360000000ULL )
		{
			wawo::thread::unique_lock<wawo::thread::mutex> ulk(m_mutex);
			u64_t _begin_time = wawo::time::curr_milliseconds();
			while (!m_should_exit) {
				u64_t now = wawo::time::curr_milliseconds();
				if ((now - _begin_time) > time) {
					break;
				}
				m_cond.wait_for(ulk, std::chrono::milliseconds(time));
			}
			u64_t _end_time = wawo::time::curr_milliseconds();
			WAWO_INFO("[APP]RunUntil return, total run time: %llu seconds", (_end_time - _begin_time));
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
