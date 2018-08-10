#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/net/io_event.hpp>
#include <wawo/net/io_event_executor.hpp>
#include <wawo/thread.hpp>

namespace wawo { namespace net {

	enum ioe_flag {
		IOE_READ = 1, //check read, sys io
		IOE_WRITE = 1<<1, //check write, sys io
		IOE_ACCEPT = 1<<2,
		IOE_CONNECT = 1<<3,
		IOE_IOCP_INIT = 1<<4,
		IOE_IOCP_DEINIT = 1<<5
	};

	class io_event_loop :
		public io_event_executor
	{
		enum state {
			S_IDLE,
			S_RUNNING,
			S_EXIT
		};
		WWRP<wawo::thread> m_th;
		volatile state m_state;
	protected:
		inline void run() {
			init();
			while (WAWO_LIKELY(S_RUNNING == m_state)) {
				io_event_executor::exec_task();
				do_poll();
			}
			deinit();
		}

	public:
		io_event_loop() :m_state(S_IDLE) {}
		inline void watch(u8_t const& flag, SOCKET const& fd, fn_io_event const& fn) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd, fn]() -> void {
				loop->do_watch(flag, fd, fn);
			});
		}
		inline void unwatch(u8_t const& flag, SOCKET const& fd) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd]() -> void {
				loop->do_unwatch(flag, fd);
			});
		}
		int start() {
			m_th = wawo::make_ref<wawo::thread>();
			int rt = m_th->start(&io_event_loop::run, WWRP<io_event_loop>(this));
			WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
			while (m_state == S_IDLE) {
				wawo::this_thread::yield();
			}
			return wawo::OK;
		}
		void stop() {
			m_state = S_EXIT;
			interrupt_wait();
			m_th->interrupt();
			m_th->join();
		}

#ifdef WAWO_IO_MODE_IOCP
		inline void IOCP_overlapped_call(u8_t const& flag, SOCKET const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) {
			WAWO_ASSERT(fd > 0);
			WWRP<io_event_loop> loop(this);
			execute([loop, flag, fd, fn_overlapped, fn]() -> void {
				loop->do_IOCP_overlapped_call(flag,fd, fn_overlapped,fn);
			});
		}
#endif
		virtual void init() {
			m_state = S_RUNNING;
			io_event_executor::init();
		}
		virtual void deinit() {
			WAWO_ASSERT(m_state == S_EXIT);
			io_event_executor::deinit();
		}
	public:
		virtual void do_poll() = 0;
		virtual void do_watch(u8_t const& flag, SOCKET const& fd, fn_io_event const& fn) = 0;
		virtual void do_unwatch(u8_t const& flag, SOCKET const& fd) = 0;

#ifdef WAWO_IO_MODE_IOCP
		virtual void do_IOCP_overlapped_call( u8_t const& flag, SOCKET const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) = 0;
#endif
	};

	typedef std::vector<WWRP<io_event_loop>> io_event_loop_vector;
	class io_event_loop_group:
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

		void execute(fn_io_event_task&& f);
		void schedule(fn_io_event_task&& f);
	};
}}
#endif