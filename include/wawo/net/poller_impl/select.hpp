#ifndef _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.hpp>
#include <wawo/net/poller_abstract.hpp>

#define WAWO_SELECT_BUCKET_MAX			1024
#define WAWO_SELECT_BUCKET_ITEM_COUNT	64
#define WAWO_SELECT_LIMIT (WAWO_SELECT_BUCKET_MAX*WAWO_SELECT_BUCKET_ITEM_COUNT)


namespace wawo { namespace net { namespace impl {

	class select:
		public poller_abstract
	{

	private:
		struct ctxs_to_check {

			ctxs_to_check():
				max_fd_v(0)
			{
				FD_ZERO( &fds_r );
				FD_ZERO( &fds_w );
				FD_ZERO( &fds_ex );
				max_fd_v = 0;
				count = 0;
			}

			void reset() {
				FD_ZERO( &fds_r );
				FD_ZERO( &fds_w );
				FD_ZERO( &fds_ex );
				max_fd_v = 0;
				count = 0;
#ifdef _DEBUG
				for (int i = 0; i < WAWO_SELECT_BUCKET_MAX; ++i) {
					ctxs[i] = NULL;
				}
#endif
			}

			WWRP<poller_ctx> ctxs[WAWO_SELECT_BUCKET_MAX];
			int count;
			fd_set fds_r;
			fd_set fds_w;
			fd_set fds_ex;
			int max_fd_v;
		};

		ctxs_to_check* m_ctxs_to_check[WAWO_SELECT_BUCKET_MAX] ;

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
//					_ctx->poll_type = T_SELECT;
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
					WAWO_CONDITION_CHECK( m_ctxs.size() < WAWO_SELECT_LIMIT ) ;
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
			int fd_added_count = 0;
			int idx = 0;

			poller_ctx_map::iterator it = m_ctxs.begin();
			while( it != m_ctxs.end() ) {
				WWRP<poller_ctx> const& ctx = it->second ;
				if(ctx->flag&IOE_READ) {
					if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
						++idx;
						WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
						fd_added_count = 0;
						m_ctxs_to_check[idx]->max_fd_v = 0;
						continue;
					}

					FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_r);
					m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

					if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
						m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
					}
					++fd_added_count;
				}

				if(ctx->flag&IOE_WRITE && ctx->fn[IOE_SLOT_WRITE] != NULL ) {
					if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
						++idx;
						WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
						fd_added_count = 0;
						m_ctxs_to_check[idx]->max_fd_v = 0;
						continue;
					}

					FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_w);
					m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

					if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
						m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
					}

					++fd_added_count;

					//for ex
					if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
						++idx;
						WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
						fd_added_count = 0;
						m_ctxs_to_check[idx]->max_fd_v = 0;
						continue;
					}

					FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_ex);
					m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

					if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
						m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
					}
					++fd_added_count;
				}
				
				++it;
			}

			for( int j=0;j<=idx;++j ) {
				if( m_ctxs_to_check[j]->count > 0 ) {
					_select_sockets( m_ctxs_to_check[j] );
					m_ctxs_to_check[j]->reset();
				}
			}
			io_event_executor::cond_wait();
		}

		void init() {
			poller_abstract::init();

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				m_ctxs_to_check[i] = new ctxs_to_check();
				WAWO_ALLOC_CHECK( m_ctxs_to_check[i] , sizeof(ctxs_to_check) );
			}
		}
		void deinit() {

			poller_abstract::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			WAWO_ASSERT(m_ctxs.size() == 0);

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				m_ctxs_to_check[i]->reset() ;
			}

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				WAWO_DELETE( m_ctxs_to_check[i]) ;
			}
			poller_abstract::deinit();

		}

		void _select_sockets( ctxs_to_check* const sockets ) {
			WAWO_ASSERT( sockets->count <= WAWO_SELECT_BUCKET_ITEM_COUNT && sockets->count >0 );

			timeval tv = {0,0};
			fd_set& fds_r = sockets->fds_r;
			fd_set& fds_w = sockets->fds_w;
			fd_set& fds_ex = sockets->fds_ex;

			int ready_c = ::select( sockets->max_fd_v , &fds_r, &fds_w, &fds_ex , &tv ); //only read now

			if( ready_c == 0 ) {
				return ;
			} else if( ready_c == -1 ) {
				//notice 10038
				WAWO_ERR("[SELECT]select error, errno: %d", socket_get_last_errno() );
				return ;
			}

			//check begin
			for( int c_idx =0; (c_idx<sockets->count)&&(ready_c>0); c_idx++ ) {
				WWRP<poller_ctx>& ctx = sockets->ctxs[c_idx] ;

				int const& fd = ctx->fd;
				if( FD_ISSET( fd, &fds_r ) ) {
					FD_CLR(fd, &fds_r);
					--ready_c ;
					fn_io_event _fn = ctx->fn[IOE_SLOT_READ];
					if ( WAWO_UNLIKELY(ctx->flag&IOE_INFINITE_WATCH_READ) == 0) {
						unwatch(IOE_READ, ctx->fd);
					}
					async_io_result r = {AIO_READ, 0,0 };
					_fn(r);
				}

				if( FD_ISSET( fd, &fds_w ) ) {
					FD_CLR(fd, &fds_w);
					--ready_c;
					fn_io_event _fn = ctx->fn[IOE_SLOT_WRITE];
					if ( WAWO_LIKELY(ctx->flag&IOE_INFINITE_WATCH_WRITE) == 0) {
						unwatch(IOE_WRITE, ctx->fd);
					}
					async_io_result r = { AIO_WRITE, 0,0 };
					_fn(r);
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

					ec = WAWO_NEGATIVE(ec);
					WAWO_ASSERT(ec != wawo::OK);

					unwatch(IOE_READ, ctx->fd);
					unwatch(IOE_WRITE, ctx->fd);//only trigger once
					async_io_result r = { AIO_WRITE, ec,0 };

					WAWO_ASSERT(_fn != NULL);
					_fn(r);
				}
			}
		}
	};
}}}
#endif//