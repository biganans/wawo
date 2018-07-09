#ifndef _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.hpp>
#include <wawo/net/poller_abstract.hpp>

namespace wawo { namespace net { namespace impl {

	class select:
		public poller_abstract
	{
		public:
			select():
				poller_abstract()
			{
			}

			~select() {}

			inline void watch_ioe( u8_t const& flag, int const& fd, fn_io_event const& fn ) {

				WAWO_ASSERT(fd >0);
				WAWO_ASSERT(fn != NULL);

				WWRP<poller_ctx> ctx;
				poller_ctx_map::iterator it = m_ctxs.find(fd);
				
				if (it == m_ctxs.end()) {
					TRACE_IOE("[io_event_loop][#%d]watch_ioe: make new, flag: %u", fd, flag );
					WWRP<poller_ctx> _ctx = wawo::make_ref<poller_ctx>();
					_ctx->fd = fd;
					_ctx->flag = 0;

					m_ctxs.insert({fd,_ctx});
					ctx = _ctx;
				}
				else {
					ctx = it->second;
				}

				ctx_update_for_watch(ctx, flag, fd, fn );
				TRACE_IOE("[io_event_loop][#%d]watch_ioe: update: %d, now: %d", fd, flag, ctx->flag);
			}

			inline void unwatch_ioe( u8_t const& flag, int const& fd) {

				WAWO_ASSERT(fd>0);
				WAWO_ASSERT(flag > 0 && flag <= 0xFF);

				poller_ctx_map::iterator it = m_ctxs.find(fd); 
				
				if (it == m_ctxs.end()) return;
				WWRP<poller_ctx> ctx = it->second ;

				ctx_update_for_unwatch(ctx, flag, fd);

				if ((ctx->flag == 0)) {
					TRACE_IOE("[select][#%d]erase fd from IOE", ctx->fd);
					m_ctxs.erase(it);
				}
			}

			void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn)
			{
				WAWO_ASSERT( flag > 0 );
				if( 0 != flag ) {
					WAWO_CONDITION_CHECK( m_ctxs.size() < FD_SETSIZE ) ;
					watch_ioe(flag, fd, fn);
				}
			}

			void do_unwatch(u8_t const& flag, int const& fd)
			{
				WAWO_ASSERT( flag > 0 );
				if( 0 != flag ) {
					unwatch_ioe( flag, fd );
				}
			}

		void do_poll() {

			if (m_ctxs.size() == 0) {
				io_event_executor::cond_wait();
			}

			fd_set fds_r;
			FD_ZERO(&fds_r);

			fd_set fds_w;
			FD_ZERO(&fds_w);

			fd_set fds_ex;
			FD_ZERO(&fds_ex);
			int max_fd_v = 0;

			poller_ctx_map::iterator it = m_ctxs.begin();
			while( it != m_ctxs.end() ) {
				WWRP<poller_ctx> const& ctx = it->second ;
				if(ctx->flag&IOE_READ) {
					//TRACE_IOE("[select]check read evt for: %d", ctx->fd );
					FD_SET((ctx->fd), &fds_r);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}

				if(ctx->flag&IOE_WRITE && ctx->fn[IOE_SLOT_WRITE] != NULL ) {
					FD_SET(ctx->fd, &fds_w);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
					FD_SET((ctx->fd), &fds_ex);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}
				
				++it;
			}

			_select(max_fd_v, fds_r, fds_w,fds_ex );
			io_event_executor::cond_wait();
		}

		void init() {
			poller_abstract::init();
		}
		void deinit() {
			poller_abstract::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			WAWO_ASSERT(m_ctxs.size() == 0);
			poller_abstract::deinit();
		}

		inline void _select(int const& max_fd_v, fd_set& fds_r, fd_set& fds_w, fd_set& fds_ex ) {
			timeval tv = {0,0};
			int ready_c = ::select( max_fd_v , &fds_r, &fds_w, &fds_ex , &tv ); //only read now

			if( ready_c == 0 ) {
				return ;
			} else if( ready_c == -1 ) {
				//notice 10038
				WAWO_ERR("[SELECT]select error, errno: %d", socket_get_last_errno() );
				return ;
			}

			//check begin
			int total_rc = 0;

			poller_ctx_map::iterator it = m_ctxs.begin();
			while (it != m_ctxs.end() && (ready_c>0) ) {
				poller_ctx_map::iterator it_cur = it;
				++it;

				WWRP<poller_ctx>& ctx = it_cur->second;
				WAWO_ASSERT(ctx != NULL);

				int const& fd = ctx->fd;
				if( FD_ISSET( fd, &fds_r ) ) {
					FD_CLR(fd, &fds_r);
					--ready_c ;
					fn_io_event _fn = ctx->fn[IOE_SLOT_READ];
					if ( WAWO_UNLIKELY(ctx->flag&IOE_INFINITE_WATCH_READ) == 0) {
						do_unwatch(IOE_READ, ctx->fd);
					}
					_fn({ AIO_READ, 0,0 });
					++total_rc;
				}

				if( FD_ISSET( fd, &fds_w ) ) {
					FD_CLR(fd, &fds_w);
					--ready_c;
					fn_io_event _fn = ctx->fn[IOE_SLOT_WRITE];
					if ( WAWO_LIKELY(ctx->flag&IOE_INFINITE_WATCH_WRITE) == 0) {
						do_unwatch(IOE_WRITE, ctx->fd);
					}
					_fn({ AIO_WRITE, 0,0 });
				}

				if( FD_ISSET(fd, &fds_ex) ) {
					FD_CLR(fd, &fds_ex);
					--ready_c ;
					WAWO_ASSERT(ctx->fd > 0);

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt == -1) {
						ec = wawo::socket_get_last_errno();
					}
					fn_io_event _fn = ctx->fn[IOE_SLOT_WRITE];
					WAWO_ASSERT(ec != wawo::OK);

					do_unwatch(IOE_READ, ctx->fd);
					do_unwatch(IOE_WRITE, ctx->fd);//only trigger once

					WAWO_ASSERT(_fn != NULL);
					_fn({ AIO_WRITE, ec,0 });
				}
			}
			if (total_rc >= 64) {
				WAWO_INFO("[IOE] total_rc reach: %d", total_rc);
			}
		}
	};
}}}
#endif//