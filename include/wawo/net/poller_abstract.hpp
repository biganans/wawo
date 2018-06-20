#ifndef _WAWO_NET_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_

#include <wawo/smart_ptr.hpp>
#include <wawo/mutex.hpp>
#include <wawo/task/scheduler.hpp>
#include <wawo/net/io_event.hpp>

#include <map>
#include <queue>

//#define ENABLE_TRACE_IOE
#ifdef ENABLE_TRACE_IOE
	#define TRACE_IOE WAWO_INFO
#else
	#define TRACE_IOE(...)
#endif

namespace wawo { namespace net {

	enum ioe_flag {
		IOE_READ	= 1, //check read, sys io
		IOE_WRITE	= 1<<1, //check write, sys io
		IOE_INFINITE_WATCH_READ		= 1<<2,
		IOE_INFINITE_WATCH_WRITE	= 1<<3,
	};

	enum poller_type {
		T_SELECT,
		T_EPOLL,
		T_IOCP,
		T_WPOLL
	};

	//static const u8_t T_SELECT = 1;
	//static const u8_t T_EPOLL = 2;
	//static const u8_t T_WPOLL = 3;

	inline u8_t get_os_default_poll_type() {
#if WAWO_ISGNU
		return T_EPOLL;
#else
	#ifdef WAWO_ENABLE_IOCP
			return T_IOCP;
	#else
			return T_SELECT;
	#endif
#endif
	}

	static const u8_t IOE_SLOT_READ		= 0;
	static const u8_t IOE_SLOT_WRITE	= 1;
	static const u8_t IOE_SLOT_MAX		= 2;

	struct poller_ctx :
		public wawo::ref_base
	{
		struct _fn_info {
			fn_io_event fn;
			fn_io_event_error err;
			WWRP<ref_base> fnctx;
		};

		int fd;
		u8_t flag;
		u8_t poll_type;
		_fn_info fn[IOE_SLOT_MAX];

		poller_ctx() :
			fd(-2),
			flag(0)
		{
			for (u8_t i = 0; i < IOE_SLOT_MAX; ++i) {
				fn[i].fn = NULL;
				fn[i].err = NULL;
				fn[i].fnctx = NULL;
			}
		}
	};

	typedef std::map<int, WWRP<poller_ctx>> poller_ctx_map;
	typedef std::pair<int, WWRP<poller_ctx>> fd_ctx_pair;

	class poller_abstract
		:public wawo::ref_base
	{
	protected:
		poller_ctx_map m_ctxs;
	public:
		poller_abstract():
			m_ctxs()
		{
		}

		inline void ctx_update_for_watch(WWRP<poller_ctx>& ctx, u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err, WWRP<ref_base> const& fnctx)
		{
			(void)fd;
			WAWO_ASSERT(flag != 0);

			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT(err != NULL);

			WAWO_ASSERT(ctx->fd == fd);
			WAWO_ASSERT((ctx->flag&flag) == 0);

			if (flag&IOE_READ) {
				ctx->flag |= IOE_READ;
				if ( flag&IOE_INFINITE_WATCH_READ ) {
					ctx->flag |= IOE_INFINITE_WATCH_READ;
				}

				TRACE_IOE("[poller_abstract][#%d]watch IOE_READ", ctx->fd);
				WAWO_ASSERT(ctx->fn[IOE_SLOT_READ].fn == NULL);
				WAWO_ASSERT(ctx->fn[IOE_SLOT_READ].err == NULL);

				ctx->fn[IOE_SLOT_READ].fn = fn;
				ctx->fn[IOE_SLOT_READ].err = err;
				ctx->fn[IOE_SLOT_READ].fnctx = fnctx;
			}

			if (flag&IOE_WRITE) {
				ctx->flag |= IOE_WRITE;
				if (flag&IOE_INFINITE_WATCH_WRITE) {
					ctx->flag |= IOE_INFINITE_WATCH_WRITE;
				}
				TRACE_IOE("[poller_abstract][#%d]watch IOE_WRITE", ctx->fd);
				WAWO_ASSERT(ctx->fn[IOE_SLOT_WRITE].fn == NULL);
				WAWO_ASSERT(ctx->fn[IOE_SLOT_WRITE].err == NULL);

				ctx->fn[IOE_SLOT_WRITE].fn = fn;
				ctx->fn[IOE_SLOT_WRITE].err = err;
				ctx->fn[IOE_SLOT_WRITE].fnctx = fnctx;
			}
		}

		inline void ctx_update_for_unwatch(WWRP<poller_ctx>& ctx, u8_t const& flag, int const& fd)
		{
			WAWO_ASSERT(ctx->fd == fd);
			(void)fd;
			if ((flag&IOE_READ) && (ctx->flag)&flag) {
				TRACE_IOE("[poller_abstract][#%d]unwatch IOE_READ", ctx->fd);
				ctx->flag &= ~(IOE_READ|IOE_INFINITE_WATCH_READ);

				ctx->fn[IOE_SLOT_READ].fn = NULL;
				ctx->fn[IOE_SLOT_READ].err = NULL;
				ctx->fn[IOE_SLOT_READ].fnctx = NULL;
			}

			if ((flag&IOE_WRITE) && (ctx->flag)&flag) {
				TRACE_IOE("[poller_abstract][#%d]unwatch IOE_WRITE", ctx->fd);
				ctx->flag &= ~(IOE_WRITE|IOE_INFINITE_WATCH_WRITE);
				ctx->fn[IOE_SLOT_WRITE].fn = NULL;
				ctx->fn[IOE_SLOT_WRITE].err = NULL;
				ctx->fn[IOE_SLOT_WRITE].fnctx = NULL;
			}
		}

		inline void ctxs_cancel_all(poller_ctx_map& ctx_map ) {
			poller_ctx_map::iterator it = ctx_map.begin();
			while (it != m_ctxs.end()) {
				WWRP<poller_ctx> const& ctx = it->second ;
				++it;

				if (ctx->fd > 0) {
					//LAST TRY
					if (ctx->fn[IOE_SLOT_READ].fn != NULL) {
						ctx->fn[IOE_SLOT_READ].fn(ctx->fn[IOE_SLOT_READ].fnctx);
					}

					if (ctx->fn[IOE_SLOT_READ].err != NULL) {
						ctx->fn[IOE_SLOT_READ].err(wawo::E_OBSERVER_EXIT, ctx->fn[IOE_SLOT_READ].fnctx);
					}

					if (ctx->fn[IOE_SLOT_WRITE].err != NULL) {
						ctx->fn[IOE_SLOT_WRITE].err(wawo::E_OBSERVER_EXIT, ctx->fn[IOE_SLOT_WRITE].fnctx);
					}
				}
			}
		}

		virtual ~poller_abstract() {}

		virtual void init() = 0;
		virtual void deinit() = 0;

	public:
		virtual void do_poll() = 0;
		virtual void watch(u8_t const& flag, int const& fd, fn_io_event const& fn,fn_io_event_error const& err, WWRP<ref_base> const& fnctx) = 0;
		virtual void unwatch(u8_t const& flag, int const& fd) = 0;
	};
}}
#endif //
