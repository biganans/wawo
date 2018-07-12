#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/net/io_event.hpp>
#include <wawo/net/io_event_executor.hpp>
#include <wawo/thread_run_object_abstract.hpp>

namespace wawo { namespace net {

	enum ioe_flag {
		IOE_READ = 1, //check read, sys io
		IOE_WRITE = 1<<1, //check write, sys io
		IOE_ACCEPT = 1<<2,
		IOE_CONNECT = 1<<3,
//		IOE_INFINITE_WATCH_READ = 1 << 4,
//		IOE_INFINITE_WATCH_WRITE = 1 << 5,
		IOE_IOCP_INIT = 1<<6,
		IOE_IOCP_DEINIT = 1<<7
	};

	class io_event_loop :
		public io_event_executor,
		public thread_run_object_abstract
	{

	protected:
		void on_start() {
			init();
		}

		void on_stop() {
			deinit();
		}

		void run() {
			io_event_executor::exec_task();
			do_poll();
		}

	public:
		inline void watch(u8_t const& flag, int const& fd, fn_io_event const& fn) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd, fn]() -> void {
				loop->do_watch(flag, fd, fn);
			});
		}
		inline void unwatch(u8_t const& flag, int const& fd) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd]() -> void {
				loop->do_unwatch(flag, fd);
			});
		}

#ifdef WAWO_IO_MODE_IOCP
		inline void IOCP_overlapped_call(u8_t const& flag, int const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd, fn_overlapped, fn]() -> void {
				loop->do_IOCP_overlapped_call(flag,fd, fn_overlapped,fn);
			});
		}
#endif
		virtual void init() {
			io_event_executor::init();
		}
		virtual void deinit() {
			io_event_executor::deinit();
		}
	public:
		virtual void do_poll() = 0;
		virtual void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn) = 0;
		virtual void do_unwatch(u8_t const& flag, int const& fd) = 0;

#ifdef WAWO_IO_MODE_IOCP
		virtual void do_IOCP_overlapped_call( u8_t const& flag, int const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) = 0;
#endif
	};

	typedef std::vector<WWRP<io_event_loop>> io_event_loop_vector;
	class io_event_loop_group :
		public wawo::singleton<io_event_loop_group>
	{

	private:
		std::atomic<int> m_curr_sys;
		std::atomic<int> m_curr_wpoll;
		io_event_loop_vector m_pollers;
		io_event_loop_vector m_wpoll_pollers;
	public:
		io_event_loop_group();
		~io_event_loop_group();
		void init(int wpoller_count = 1);
		WWRP<io_event_loop> next(bool const& return_wpoller = false);
		void deinit();
	};

}}
#endif