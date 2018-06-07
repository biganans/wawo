#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/net/io_event_executor.hpp>
#include <wawo/net/socket_observer.hpp>
#include <wawo/net/wcp.hpp>

namespace wawo { namespace net {
	class io_event_loop :
		public io_event_executor
	{
		WWRP<socket_observer> m_observer;
		u8_t m_observer_type;
	public:
		io_event_loop(u8_t t = get_os_default_poll_type()) :
			m_observer_type(t)
		{}
		~io_event_loop() {}

		u8_t get_type() const { return m_observer_type; }

		void on_start() {
			io_event_executor::on_start();

			WAWO_ASSERT(m_observer == NULL);
			m_observer = wawo::make_ref<socket_observer>(m_observer_type);
			WAWO_ALLOC_CHECK(m_observer, sizeof(socket_observer));
			m_observer->init();
		}

		void on_stop() {
			io_event_executor::on_stop();

			WAWO_ASSERT(m_observer != NULL);
			m_observer->deinit();
			m_observer = NULL;
		}

		void run() {
			io_event_executor::run();
			m_observer->update();
			io_event_executor::run();
			wawo::this_thread::nsleep(observer_checker_interval);
		}

		void watch(u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err) {
			m_observer->watch(flag, fd, fn, err);
		}
		void unwatch(u8_t const& flag, int const& fd) {
			m_observer->unwatch(flag, fd);
		}
	};

	typedef std::vector<WWRP<io_event_loop>> io_event_loop_vector;
	class io_event_loop_group :
		public wawo::singleton<io_event_loop_group>
	{
	private:
		std::atomic<int> m_curr_sys;
		std::atomic<int> m_curr_wpoll;
		io_event_loop_vector m_event_loops;
		io_event_loop_vector m_wpoll_event_loops;
	public:
		io_event_loop_group()
			:m_curr_sys(0), m_curr_wpoll(0)
		{}
		~io_event_loop_group() {}
		void init(int wpoller_count = 1) {
			int i = std::thread::hardware_concurrency();
			int sys_i = i - wpoller_count;
			if (sys_i <= 0) {
				sys_i = 1;
			}

			while (sys_i-- > 0) {
				WWRP<io_event_loop> o = wawo::make_ref<io_event_loop>();
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_event_loops.push_back(o);
			}

			wpoller_count = WAWO_MIN(wpoller_count, 4);
			wpoller_count = WAWO_MAX(wpoller_count, 1);

			if (wpoller_count > 0) {
				wcp::instance()->start();
			}

			while (wpoller_count-- > 0) {
				WWRP<io_event_loop> o = wawo::make_ref<io_event_loop>(T_WPOLL);
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_wpoll_event_loops.push_back(o);
			}
		}
		WWRP<io_event_loop> next(bool const& return_wpoller = false) {
			if (return_wpoller) {
				int i = m_curr_wpoll.load() % m_wpoll_event_loops.size();
				wawo::atomic_increment(&m_curr_wpoll);
				return m_wpoll_event_loops[i% m_wpoll_event_loops.size()];
			}
			else {
				int i = m_curr_sys.load() % m_event_loops.size();
				wawo::atomic_increment(&m_curr_sys);
				return m_event_loops[i% m_event_loops.size()];
			}
		}
		void deinit() {
			if (m_wpoll_event_loops.size()) {
				wcp::instance()->stop();
				std::for_each(m_wpoll_event_loops.begin(), m_wpoll_event_loops.end(), [](WWRP<io_event_loop> const& o) {
					o->stop();
				});
				m_wpoll_event_loops.clear();
			}

			std::for_each(m_event_loops.begin(), m_event_loops.end(), [](WWRP<io_event_loop> const& o) {
				o->stop();
			});
			m_event_loops.clear();
		}
	};

}}
#endif