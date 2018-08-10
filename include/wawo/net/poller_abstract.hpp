#ifndef _WAWO_NET_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_

#include <wawo/smart_ptr.hpp>
#include <wawo/mutex.hpp>
#include <wawo/net/io_event.hpp>
#include <wawo/net/io_event_loop.hpp>

#include <map>
#include <queue>

namespace wawo { namespace net {

	enum poller_type {
		T_SELECT,
		T_EPOLL,
		T_IOCP,
		T_WPOLL
	};

	inline poller_type get_poll_type() {
#ifdef WAWO_ENABLE_EPOLL
		return T_EPOLL;
#elif defined(WAWO_IO_MODE_IOCP)
		return T_IOCP;
#else
		return T_SELECT;
#endif
	}

	static const u8_t IOE_SLOT_READ		= 0;
	static const u8_t IOE_SLOT_WRITE	= 1;
	static const u8_t IOE_SLOT_MAX		= 2;

	struct poller_ctx :
		public wawo::ref_base
	{
		SOCKET fd;
		fn_io_event fn[IOE_SLOT_MAX];
		u8_t flag;

		poller_ctx() :
			fd(wawo::E_INVALID_SOCKET),
			flag(0)
		{
			for (u8_t i = 0; i < IOE_SLOT_MAX; ++i) {
				fn[i] = nullptr;
			}
		}
	};

	typedef std::map<SOCKET, WWRP<poller_ctx>> poller_ctx_map;
	typedef std::pair<SOCKET, WWRP<poller_ctx>> fd_ctx_pair;

	class poller_abstract:
		public io_event_loop
	{
	protected:
		poller_ctx_map m_ctxs;
	public:
		poller_abstract():
			m_ctxs()
		{
		}

		inline void ctx_update_for_watch(WWRP<poller_ctx>& ctx, u8_t const& flag, fn_io_event const& fn)
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

		inline void ctx_update_for_unwatch(WWRP<poller_ctx>& ctx, u8_t const& flag)
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

		static inline void ctxs_cancel_all(poller_ctx_map& ctx_map ) {
			poller_ctx_map::iterator it = ctx_map.begin();
			while (it != ctx_map.end()) {
				WWRP<poller_ctx> ctx = it->second ;
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

		virtual ~poller_abstract() {}

		virtual void init() {
			io_event_loop::init();
		}
		virtual void deinit() {
			io_event_loop::deinit();
		}
	public:
		//virtual void do_poll() = 0;
		//virtual void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn) = 0;
		//virtual void do_unwatch(u8_t const& flag, int const& fd) = 0;
	};
}}
#endif //
