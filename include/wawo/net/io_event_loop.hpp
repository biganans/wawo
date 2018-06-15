#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/net/io_event_executor.hpp>
#include <wawo/net/observer_abstract.hpp>

namespace wawo { namespace net {

	class io_event_loop :
		public ref_base,
		public wawo::thread_run_object_abstract,
		public io_event_executor
	{
		WWRP<observer_abstract> m_observer;
		u8_t m_observer_type;
	public:
		io_event_loop(u8_t t = get_os_default_poll_type()) :
			m_observer_type(t)
		{}
		~io_event_loop() {}

		u8_t get_type() const { return m_observer_type; }

		void init_observer();
		void deinit_observer();

		void on_start() {
			io_event_executor::init();
			init_observer();
		}

		void on_stop() {
			io_event_executor::deinit();
			deinit_observer();
		}

		void run() {
			io_event_executor::exec_task();
			m_observer->do_poll();
			io_event_executor::exec_task();
			wawo::this_thread::nsleep(1);
		}

		inline void watch(u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err) {
			WAWO_ASSERT(fd > 0);
			io_event_executor::schedule([observer=m_observer,flag,fd,fn,err]() -> void {
				observer->watch(flag,fd,fn,err);
			});
		}
		inline void unwatch(u8_t const& flag, int const& fd) {
			WAWO_ASSERT(fd > 0);
			io_event_executor::schedule([observer = m_observer, flag, fd]() -> void {
				observer->unwatch(flag, fd);
			});
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
		io_event_loop_group();
		~io_event_loop_group(); 
		void init(int wpoller_count = 1);
		WWRP<io_event_loop> next(bool const& return_wpoller = false);
		void deinit();
	};

}}
#endif