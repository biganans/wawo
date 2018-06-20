#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/net/io_event_executor.hpp>
#include <wawo/net/poller_abstract.hpp>

namespace wawo { namespace net {

	class io_event_loop :
		public ref_base,
		public wawo::thread_run_object_abstract,
		public io_event_executor
	{
		WWRP<poller_abstract> m_poller;
		u8_t m_poller_type;
	public:
		io_event_loop(u8_t t = get_os_default_poll_type()) :
			m_poller_type(t)
		{}
		~io_event_loop() {}

		u8_t get_type() const { return m_poller_type; }

		void init_poller();
		void deinit_poller();

		void on_start() {
			io_event_executor::init();
			init_poller();
		}

		void on_stop() {
			io_event_executor::deinit();
			deinit_poller();
		}

		void run() {
			io_event_executor::exec_task();
			m_poller->do_poll();
			io_event_executor::wait();
		}

		inline void watch(u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err, WWRP<ref_base> const& fnctx = NULL ) {
			WAWO_ASSERT(fd > 0);
			io_event_executor::execute([poller=m_poller,flag,fd,fn,err,fnctx]() -> void {
				poller->watch(flag,fd,fn,err,fnctx );
			});
		}
		inline void unwatch(u8_t const& flag, int const& fd) {
			WAWO_ASSERT(fd > 0);
			io_event_executor::execute([poller=m_poller, flag, fd]() -> void {
				poller->unwatch(flag, fd);
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