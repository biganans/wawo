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
		IOE_WRITE	= 1 << 1, //check write, sys io
		IOE_INFINITE_WATCH_READ		= 1<<2,
		IOE_INFINITE_WATCH_WRITE	= 1<<3,
	};

	static const u8_t T_SELECT = 1;
	static const u8_t T_EPOLL = 2;
	static const u8_t T_WPOLL = 3;

	inline u8_t get_os_default_poll_type() {
#if WAWO_ISGNU
		return T_EPOLL;
#else
		return T_SELECT;
#endif
	}

	static const u8_t IOE_SLOT_READ		= 0;
	static const u8_t IOE_SLOT_WRITE	= 1;
	static const u8_t IOE_SLOT_MAX		= 2;

	struct observer_ctx :
		public wawo::ref_base
	{
		struct _fn_info {
			fn_io_event fn;
			fn_io_event_error err;
		};

		observer_ctx() :
			flag(0),
			fd(-2)
		{
			for (u8_t i = 0; i < IOE_SLOT_MAX; ++i) {
				fn_info[i].fn = NULL;
				fn_info[i].err = NULL;
			}
		}

		u8_t flag;
		u8_t poll_type;
		int fd;
		_fn_info fn_info[IOE_SLOT_MAX];;
	};

	struct fn_read_info {
		fn_io_event fn;
		WWRP<ref_base> cookie;
		fn_read_info() :
			fn(NULL)
		{}
		~fn_read_info() {}
	};

	struct fn_error_info {
		fn_io_event_error fn;
		fn_error_info() :
			fn(NULL)
		{}
		~fn_error_info() {}
	};

	struct RE {
		fn_read_info read;
		fn_error_info rd_err;
		fn_error_info wr_err;
	};

	typedef std::map<int, WWRP<observer_ctx>> observer_ctx_map;
	typedef std::pair<int, WWRP<observer_ctx>> fd_ctx_pair;

	class observer_abstract
		:public wawo::ref_base
	{
	protected:
		observer_ctx_map m_ctxs;
	public:
		observer_abstract():
			m_ctxs()
		{
		}

		inline void ctx_update_for_watch(WWRP<observer_ctx>& ctx, u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err)
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

				TRACE_IOE("[observer_abstract][#%d]watch IOE_READ", ctx->fd);
				WAWO_ASSERT(ctx->fn_info[IOE_SLOT_READ].fn == NULL);
				WAWO_ASSERT(ctx->fn_info[IOE_SLOT_READ].err == NULL);

				ctx->fn_info[IOE_SLOT_READ].fn = fn;
				ctx->fn_info[IOE_SLOT_READ].err = err;
			}

			if (flag&IOE_WRITE) {
				ctx->flag |= IOE_WRITE;
				if (flag&IOE_INFINITE_WATCH_WRITE) {
					ctx->flag |= IOE_INFINITE_WATCH_WRITE;
				}
				TRACE_IOE("[observer_abstract][#%d]watch IOE_WRITE", ctx->fd);
				WAWO_ASSERT(ctx->fn_info[IOE_SLOT_WRITE].fn == NULL);
				WAWO_ASSERT(ctx->fn_info[IOE_SLOT_WRITE].err == NULL);

				ctx->fn_info[IOE_SLOT_WRITE].fn = fn;
				ctx->fn_info[IOE_SLOT_WRITE].err = err;
			}
		}

		inline void ctx_update_for_unwatch(WWRP<observer_ctx>& ctx, u8_t const& flag, int const& fd)
		{
			WAWO_ASSERT(ctx->fd == fd);
			(void)fd;
			if ((flag&IOE_READ) && (ctx->flag)&flag) {
				TRACE_IOE("[observer_abstract][#%d]unwatch IOE_READ", ctx->fd);
				ctx->flag &= ~(IOE_READ|IOE_INFINITE_WATCH_READ);

				ctx->fn_info[IOE_SLOT_READ].fn = NULL;
				ctx->fn_info[IOE_SLOT_READ].err = NULL;
			}

			if ((flag&IOE_WRITE) && (ctx->flag)&flag) {
				TRACE_IOE("[observer_abstract][#%d]unwatch IOE_WRITE", ctx->fd);
				ctx->flag &= ~(IOE_WRITE|IOE_INFINITE_WATCH_WRITE);
				ctx->fn_info[IOE_SLOT_WRITE].fn = NULL;
				ctx->fn_info[IOE_SLOT_WRITE].err = NULL;
			}
		}

		inline void ctxs_cancel_all(observer_ctx_map& ctx_map ) {
			observer_ctx_map::iterator it = ctx_map.begin();
			while (it != m_ctxs.end()) {
				WWRP<observer_ctx> const& ctx = it->second ;
				++it;

				if (ctx->fd > 0) {
					RE _re;

					_re.rd_err.fn = ctx->fn_info[IOE_SLOT_READ].err;
					_re.wr_err.fn = ctx->fn_info[IOE_SLOT_WRITE].err;

					int ec = wawo::E_OBSERVER_EXIT;
					wawo::task::fn_task_void _lambda = [_re, ec]() -> void {
						if (_re.read.fn != NULL) {
							_re.read.fn();
						}
						if (_re.rd_err.fn != NULL) {
							_re.rd_err.fn(ec);
						}
						if (_re.wr_err.fn != NULL) {
							_re.wr_err.fn(ec);
						}
					};

					_lambda();
				}
			}
		}

		virtual ~observer_abstract() {}

		virtual void init() {};
		virtual void deinit() {};

	public:
		virtual void do_poll() = 0;
		virtual void watch(u8_t const& flag, int const& fd, fn_io_event const& fn,fn_io_event_error const& err ) = 0;
		virtual void unwatch(u8_t const& flag, int const& fd) = 0;
	};
}}
#endif //
