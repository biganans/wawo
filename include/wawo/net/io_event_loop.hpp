#ifndef _WAWO_NET_IO_EVENT_LOOP_HPP
#define _WAWO_NET_IO_EVENT_LOOP_HPP

#include <wawo/thread.hpp>

#include <wawo/net/io_event.hpp>
#include <wawo/net/io_event_executor.hpp>
#include <wawo/net/address.hpp>

#include <map>

namespace wawo { namespace net {

	enum io_poller_type {
		T_SELECT,
		T_EPOLL,
		T_IOCP,
		T_WPOLL,
		T_POLLER_MAX
	};

	static inline io_poller_type get_poller_type() {
#ifdef WAWO_IO_MODE_IOCP
		return T_IOCP;
#elif defined(WAWO_IO_MODE_EPOLL)
		return T_EPOLL;
#elif defined(WAWO_IO_MODE_SELECT)
		return T_SELECT;
#else
	#error
#endif
	}

	static inline io_poller_type determin_type_by_protocol(wawo::net::s_protocol const& p) {
		if (p == P_WCP) {
			return T_WPOLL;
		}
		return get_poller_type();
	}

	enum ioe_flag {
		IOE_READ = 1, //check read, sys io
		IOE_WRITE = 1<<1, //check write, sys io
		IOE_ACCEPT = 1<<2,
		IOE_CONNECT = 1<<3,
		IOE_IOCP_INIT = 1<<4,
		IOE_IOCP_DEINIT = 1<<5
	};

	static const u8_t IOE_SLOT_READ = 0;
	static const u8_t IOE_SLOT_WRITE = 1;
	static const u8_t IOE_SLOT_MAX = 2;

	struct watch_ctx :
		public wawo::ref_base
	{
		SOCKET fd;
		fn_io_event fn[IOE_SLOT_MAX];
		u8_t flag;

		watch_ctx() :
			fd(wawo::E_INVALID_SOCKET),
			flag(0)
		{
			for (u8_t i = 0; i < IOE_SLOT_MAX; ++i) {
				fn[i] = nullptr;
			}
		}
	};

	typedef std::map<SOCKET, WWRP<watch_ctx>> watch_ctx_map;
	typedef std::pair<SOCKET, WWRP<watch_ctx>> watch_ctx_map_pair;

	class io_event_loop :
		public io_event_executor
	{
		enum state {
			S_IDLE,
			S_RUNNING,
			S_EXIT
		};

protected:
		watch_ctx_map m_ctxs;
private:
		WWRP<wawo::thread> m_th;
		io_poller_type m_type;
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
		io_event_loop( io_poller_type const& t) : m_type(t), m_state(S_IDLE) {}
		__WW_FORCE_INLINE io_poller_type const& poller_type() const { return m_type; }
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

		inline void ctx_update_for_watch(WWRP<watch_ctx>& ctx, u8_t const& flag, fn_io_event const& fn)
		{
			WAWO_ASSERT(flag != 0);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT((ctx->flag&flag) == 0);

			if (flag&IOE_READ) {
				ctx->flag |= IOE_READ;
				WAWO_TRACE_IOE("[io_event_loop][#%d]watch IOE_READ", ctx->fd);
#ifdef _DEBUG
				WAWO_ASSERT(ctx->fn[IOE_SLOT_READ] == NULL);
#endif
				ctx->fn[IOE_SLOT_READ] = fn;
	}

			if (flag&IOE_WRITE) {
				ctx->flag |= IOE_WRITE;
				WAWO_TRACE_IOE("[io_event_loop][#%d]watch IOE_WRITE", ctx->fd);
#ifdef _DEBUG
				WAWO_ASSERT(ctx->fn[IOE_SLOT_WRITE] == NULL);
#endif
				ctx->fn[IOE_SLOT_WRITE] = fn;
			}
}

		inline void ctx_update_for_unwatch(WWRP<watch_ctx>& ctx, u8_t const& flag)
		{
			if ((flag&IOE_READ) && (ctx->flag)&flag) {
				WAWO_TRACE_IOE("[io_event_loop][#%d]unwatch IOE_READ", ctx->fd);
				ctx->flag &= ~(IOE_READ);
#ifdef _DEBUG
				ctx->fn[IOE_SLOT_READ] = NULL;
#endif
			}

			if ((flag&IOE_WRITE) && (ctx->flag)&flag) {
				WAWO_TRACE_IOE("[io_event_loop][#%d]unwatch IOE_WRITE", ctx->fd);
				ctx->flag &= ~(IOE_WRITE);
#ifdef _DEBUG
				ctx->fn[IOE_SLOT_WRITE] = NULL;
#endif
			}
		}

		static inline void ctxs_cancel_all(watch_ctx_map& ctx_map) {
			watch_ctx_map::iterator it = ctx_map.begin();
			while (it != ctx_map.end()) {
				WWRP<watch_ctx> ctx = it->second;
				++it;

				if (ctx->fd > 0) {
					if (ctx->fn[IOE_SLOT_READ] != NULL) {
						ctx->fn[IOE_SLOT_READ]({ AIO_READ, ctx->fd,wawo::E_OBSERVER_EXIT,0 });
					}
					if (ctx->fn[IOE_SLOT_WRITE] != NULL) {
						ctx->fn[IOE_SLOT_WRITE]({ AIO_WRITE, ctx->fd,wawo::E_OBSERVER_EXIT,0 });
					}
				}
			}
			ctx_map.clear();
		}

#ifdef WAWO_IO_MODE_IOCP
		inline void IOCP_overlapped_call(u8_t const& flag, SOCKET const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) {
			WAWO_ASSERT(fd != wawo::E_INVALID_SOCKET );
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
		std::atomic<int> m_curr_idx[T_POLLER_MAX];
		io_event_loop_vector m_pollers[T_POLLER_MAX];
	public:
		io_event_loop_group();
		~io_event_loop_group();
		void init(int wpoller_count = 1);
		void deinit();
		WWRP<io_event_loop> next(io_poller_type const& t);

		void execute(fn_io_event_task&& f);
		void schedule(fn_io_event_task&& f);
	};
}}
#endif