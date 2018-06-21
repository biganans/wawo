#ifndef _WAWO_NET_OBSERVER_IMPL_WPOLL_HPP_
#define _WAWO_NET_OBSERVER_IMPL_WPOLL_HPP_

#include <wawo/net/io_event_executor.hpp>
#include <wawo/net/poller_abstract.hpp>
#include <wawo/net/wcp.hpp>

namespace wawo { namespace net { namespace impl {
	class wpoll :
		public poller_abstract
	{
		int m_wpHandle;

	public:
		wpoll() :
			poller_abstract(),
			m_wpHandle(0)
		{
		}

		~wpoll() {}

		void init() {
			poller_abstract::init();
			m_wpHandle = wcp::instance()->wpoll_create();
			WAWO_ASSERT( m_wpHandle > 0 );
		}

		void deinit() {
			poller_abstract::deinit();
			WAWO_ASSERT( m_wpHandle > 0 );

			poller_abstract::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			wcp::instance()->wpoll_close(m_wpHandle);
			m_wpHandle = -1;
		}

		virtual void do_watch(u8_t const& flag, int const& fd,fn_io_event const& fn,fn_io_event_error const& err, WWRP<ref_base> const& fnctx ) {

			WAWO_ASSERT(flag>0);
			WAWO_ASSERT(fd>0);
			WAWO_ASSERT(m_wpHandle > 0);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT(err != NULL);

			WAWO_ASSERT(flag&(IOE_READ | IOE_WRITE));

			WWRP<poller_ctx> ctx;
			typename poller_ctx_map::iterator it = m_ctxs.find(fd); 
			
			bool _to_add = false;
			if (it == m_ctxs.end()) {
				WWRP<poller_ctx> _ctx = wawo::make_ref<poller_ctx>();
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
				err(ret, NULL);
				WAWO_ERR("[WPOLL]WPOLL CTL ADD FAILED: %d", ret);
				return;
			}

			WAWO_ASSERT( (ctx->flag&flag) == 0);
			ctx_update_for_watch(ctx, flag, fd, fn, err,fnctx );
			if (_to_add) {
				m_ctxs.insert({fd, ctx});
			}
			TRACE_IOE("[WPOLL][##%d][#%d][watch]wpoll op success,op flag: %d, new flag: %d", m_wpHandle, fd, flag, ctx->flag);
		}

		virtual void do_unwatch(u8_t const&flag, int const& fd) {
			WAWO_ASSERT( flag>0 );
			WAWO_ASSERT( fd>0 );
			WAWO_ASSERT(m_wpHandle > 0);

			poller_ctx_map::iterator it = m_ctxs.find(fd);
			
			if (it == m_ctxs.end()) { return; }

			WWRP<poller_ctx> ctx = it->second ;

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

		void do_poll() {
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
				WWRP<poller_ctx> const& ctx = wawo::static_pointer_cast<poller_ctx>(wpEvents[i].cookie);
				WAWO_ASSERT(ctx->fd>0);

				TRACE_IOE("[WPOLL][##%d][#%d]EVT: events(%d)", m_wpHandle, wpEvents[i].fd, events);

				if (events&WPOLLERR) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLERR", m_wpHandle, ctx->fd);
					events &= ~WPOLLERR;
					bool has_in = false;
					if (events&WPOLLIN) {
						events &= ~WPOLLIN;
						has_in = true;
					}

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = wcp::instance()->getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt<0) {
						ec = wawo::socket_get_last_errno();
					}
					WAWO_ASSERT(ec != 0);
					ec = WAWO_NEGATIVE(ec);

					if (has_in && ctx->fn[IOE_SLOT_READ].fn ) {
						ctx->fn[IOE_SLOT_READ].fn(ctx->fn[IOE_SLOT_READ].fnctx);
					}

					if (ctx->fn[IOE_SLOT_READ].err) {
						ctx->fn[IOE_SLOT_READ].err(ec,ctx->fn[IOE_SLOT_READ].fnctx);
					}
					if (ctx->fn[IOE_SLOT_WRITE].err) {
						ctx->fn[IOE_SLOT_WRITE].err(ec, ctx->fn[IOE_SLOT_WRITE].fnctx);
					}

					unwatch(IOE_WRITE, ctx->fd);
					unwatch(IOE_READ, ctx->fd);
					continue;
				}

				if (events& WPOLLIN) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLIN", m_wpHandle, wpEvents[i].fd);
					events &= ~WPOLLIN;

					fn_io_event fn = ctx->fn[IOE_SLOT_READ].fn;
					WWRP<ref_base> fnctx = ctx->fn[IOE_SLOT_READ].fnctx;

					WAWO_ASSERT(fn != NULL);
					if (WAWO_LIKELY(!(ctx->flag&IOE_INFINITE_WATCH_READ))) {
						unwatch(IOE_READ, ctx->fd);
					}
					fn(fnctx);
				}

				if (events&WPOLLOUT) {
					//TRACE_IOE("[WPOLL][##%d][#%d]EVT: WPOLLOUT", m_wpHandle, ctx->fd);
					events &= ~WPOLLOUT;

					fn_io_event fn = ctx->fn[IOE_SLOT_WRITE].fn;
					WWRP<ref_base> fnctx = ctx->fn[IOE_SLOT_WRITE].fnctx;
					WAWO_ASSERT(fn != NULL);

					if (WAWO_LIKELY(!(ctx->flag&IOE_INFINITE_WATCH_WRITE))) {
						unwatch(IOE_WRITE, ctx->fd);
					}

					fn(fnctx);
				}

				WAWO_ASSERT(events == 0);
			}
		}
	};
}}}
#endif
