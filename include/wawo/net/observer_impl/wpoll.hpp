#ifndef _WAWO_NET_OBSERVER_IMPL_WPOLL_HPP_
#define _WAWO_NET_OBSERVER_IMPL_WPOLL_HPP_

#include <wawo/net/observer_abstract.hpp>
#include <wawo/net/wcp.hpp>

namespace wawo { namespace net { namespace observer_impl {
	class wpoll :
		public observer_abstract
	{
		int m_wpHandle;

	public:
		wpoll() :
			observer_abstract(),
			m_wpHandle(0)
		{
		}

		~wpoll() {}

		void init() {
			m_wpHandle = wcp::instance()->wpoll_create();
			WAWO_ASSERT( m_wpHandle > 0 );
		}

		void deinit() {
			WAWO_ASSERT( m_wpHandle > 0 );

			observer_abstract::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			wcp::instance()->wpoll_close(m_wpHandle);
			m_wpHandle = -1;
		}

		virtual void watch(u8_t const& flag, int const& fd,  WWRP<ref_base> const& cookie,fn_io_event const& fn,fn_io_event_error const& err) {

			WAWO_ASSERT(flag>0);
			WAWO_ASSERT(fd>0);
			WAWO_ASSERT(m_wpHandle > 0);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT(err != NULL);

			WAWO_ASSERT(flag&(IOE_READ | IOE_WRITE));

			WWRP<observer_ctx> ctx;
			typename observer_ctx_map::iterator it = m_ctxs.find(fd); 
			
			bool _to_add = false;
			if (it == m_ctxs.end()) {
				WWRP<observer_ctx> _ctx = wawo::make_ref<observer_ctx>();
				_ctx->fd = fd;
				_ctx->poll_type = T_WPOLL;
				_ctx->flag = 0;

				ctx = _ctx;
				_to_add = true;
			}
			else {
				ctx = it->second ;
			}

			wpoll_event evt = {fd, static_cast<u32_t>(flag&(IOE_READ|IOE_WRITE)), ctx};

			int ret = wcp::instance()->wpoll_ctl( m_wpHandle, WPOLL_CTL_ADD, evt );
			if (ret != wawo::OK) {

				wawo::task::fn_lambda _lambda = [err, ret, cookie]() -> void {
					err(ret, cookie);
				};
				WAWO_SCHEDULER->schedule(_lambda);

				WAWO_ERR("[WPOLL]WPOLL CTL ADD FAILED: %d", ret);
				return;
			}

			WAWO_ASSERT( (ctx->flag&flag) == 0);
			ctx_update_for_watch(ctx, flag, fd, cookie, fn, err);
			if (_to_add) {
				m_ctxs.insert({fd, ctx});
			}
			TRACE_IOE("[WPOLL][##%d][#%d][watch]wpoll op success,op flag: %d, new flag: %d", m_wpHandle, fd, flag, ctx->flag);
		}

		virtual void unwatch(u8_t const&flag, int const& fd) {
			WAWO_ASSERT( flag>0 );
			WAWO_ASSERT( fd>0 );
			WAWO_ASSERT(m_wpHandle > 0);

			observer_ctx_map::iterator it = m_ctxs.find(fd);
			
			if (it == m_ctxs.end()) { return; }

			WWRP<observer_ctx> ctx = it->second ;

			wpoll_event evt;
			evt.evts = (flag& (IOE_READ|IOE_WRITE));
			evt.fd = fd;

			int ret = wcp::instance()->wpoll_ctl(m_wpHandle, WPOLL_CTL_DEL, evt);
			if (ret < 0) {
				WAWO_ERR("[WPOLL]WPOLL CTL DEL FAILED: %d", ret);
				m_ctxs.erase(it);
				return;
			}

			ctx_update_for_unwatch(ctx, flag, fd);
			if (ctx->flag == 0) {
				m_ctxs.erase(it);
			}
			TRACE_IOE("[WPOLL][##%d][#%d][unwatch]wpoll op success, op flag: %d, new flag: %d", m_wpHandle, fd, flag, ctx->flag);
		}

		void check_ioe() {
			WAWO_ASSERT(m_wpHandle > 0);
			wpoll_event wpEvents[1024];
			int nEvents = wcp::instance()->wpoll_wait(m_wpHandle, wpEvents, 1024);

			if (nEvents<0) {
				WAWO_ERR("[WPOLL]wpoll wait event failed!, errno: %d", m_wpHandle, socket_get_last_errno());
				return;
			}

			for (int i = 0; i < nEvents; ++i) {

				WAWO_ASSERT(wpEvents[i].cookie != NULL);
				u32_t events = wpEvents[i].evts;
				WWRP<observer_ctx> const& ctx = wawo::static_pointer_cast<observer_ctx>(wpEvents[i].cookie);
				WAWO_ASSERT(ctx->fd>0);

				TRACE_IOE("[WPOLL][##%d][#%d]EVT: events(%d)", m_wpHandle, wpEvents[i].fd, events);

				if (events&WPOLLERR) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLERR", m_wpHandle, ctx->fd);
					events &= ~WPOLLERR;

					RE _re;

					if (events&WPOLLIN) {
						events &= ~WPOLLIN;

						WAWO_ASSERT(ctx->fn_info[IOE_SLOT_READ].fn != NULL);
						WAWO_ASSERT(ctx->fn_info[IOE_SLOT_READ].cookie != NULL);

						_re.read.fn = ctx->fn_info[IOE_SLOT_READ].fn;
						_re.read.cookie = ctx->fn_info[IOE_SLOT_READ].cookie;
					}

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = wcp::instance()->getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt<0) {
						ec = wawo::socket_get_last_errno();
					}
					WAWO_ASSERT(ec != 0);
					ec = WAWO_NEGATIVE(ec);

					_re.rd_err.fn = ctx->fn_info[IOE_SLOT_READ].err;
					_re.rd_err.cookie = ctx->fn_info[IOE_SLOT_READ].cookie;

					_re.wr_err.fn = ctx->fn_info[IOE_SLOT_WRITE].err;
					_re.wr_err.cookie = ctx->fn_info[IOE_SLOT_WRITE].cookie;

					unwatch(IOE_WRITE, ctx->fd);
					unwatch(IOE_READ, ctx->fd);

					wawo::task::fn_lambda _lambda = [_re, ec]() -> void {
						if (_re.read.fn != NULL) {
							_re.read.fn(_re.read.cookie);
						}
						if (_re.rd_err.fn != NULL) {
							_re.rd_err.fn(ec, _re.rd_err.cookie);
						}
						if (_re.wr_err.fn != NULL) {
							_re.wr_err.fn(ec, _re.wr_err.cookie);
						}
					};

					WAWO_SCHEDULER->schedule(_lambda);
					continue;
				}

				if (events& WPOLLIN) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLIN", m_wpHandle, wpEvents[i].fd);
					events &= ~WPOLLIN;

					fn_io_event& fn = ctx->fn_info[IOE_SLOT_READ].fn;
					WWRP<ref_base>& cookie = ctx->fn_info[IOE_SLOT_READ].cookie;

					WAWO_ASSERT(fn != NULL);
					WAWO_ASSERT(cookie != NULL);

					WWRP<wawo::task::task> _t = wawo::make_ref<wawo::task::task>(fn, cookie);
					if (WAWO_LIKELY(!(ctx->flag&IOE_INFINITE_WATCH_READ))) {
						unwatch(IOE_READ, ctx->fd);
					}
					WAWO_SCHEDULER->schedule(_t);
				}

				if (events&WPOLLOUT) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLOUT", m_wpHandle, ctx->fd);
					events &= ~WPOLLOUT;

					fn_io_event& fn = ctx->fn_info[IOE_SLOT_WRITE].fn;
					WWRP<ref_base>& cookie = ctx->fn_info[IOE_SLOT_WRITE].cookie;
					WAWO_ASSERT(fn != NULL);
					WAWO_ASSERT(cookie != NULL);

					WWRP<wawo::task::task> _t = wawo::make_ref<wawo::task::task>(fn, cookie);

					if (WAWO_LIKELY(!(ctx->flag&IOE_INFINITE_WATCH_WRITE))) {
						unwatch(IOE_WRITE, ctx->fd);
					}
					WAWO_SCHEDULER->schedule(_t);
				}

				WAWO_ASSERT(events == 0);
			}
		}
	};
}}}
#endif
