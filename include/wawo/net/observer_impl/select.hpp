#ifndef _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.hpp>
#include <wawo/net/observer_abstract.hpp>

#define WAWO_SELECT_BUCKET_MAX			1024
#define WAWO_SELECT_BUCKET_ITEM_COUNT	64
#define WAWO_SELECT_LIMIT (WAWO_SELECT_BUCKET_MAX*WAWO_SELECT_BUCKET_ITEM_COUNT)


namespace wawo { namespace net { namespace observer_impl {

	using namespace wawo::net ;
	using namespace wawo::thread;

	struct io_select_task :
		public wawo::task::task
	{
		WAWO_DECLARE_NONCOPYABLE(io_select_task)

	public:
		WWRP<observer_ctx> ctx;
		u8_t id;
		explicit io_select_task(fn_io_event const& fn, WWRP<observer_ctx> ctx_, u8_t const& id_) :
			task(fn),
			ctx(ctx_),
			id(id_)
		{
		}

		void run();
	};

	class select:
		public observer_abstract
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

			WWRP<observer_ctx> ctxs[WAWO_SELECT_BUCKET_MAX];
			int count;
			fd_set fds_r;
			fd_set fds_w;
			fd_set fds_ex;
			int max_fd_v;
		};

		ctxs_to_check* m_ctxs_to_check[WAWO_SELECT_BUCKET_MAX] ;

		public:
			select():
				observer_abstract()
			{
			}

			~select() {}

			inline void watch_ioe( u8_t const& flag, int const& fd, fn_io_event const& fn,fn_io_event_error const& err ) {

				WAWO_ASSERT(fd >0);
				WAWO_ASSERT(fn != NULL);

				WWRP<observer_ctx> ctx;
				observer_ctx_map::iterator it = m_ctxs.find(fd);
				
				if (it == m_ctxs.end()) {
					TRACE_IOE("[observer_abstract][#%d]watch_ioe: make new, flag: %u", fd, flag );
					WWRP<observer_ctx> _ctx = wawo::make_ref<observer_ctx>();
					_ctx->fd = fd;
					_ctx->poll_type = T_SELECT;
					_ctx->flag = 0;

					m_ctxs.insert({fd,_ctx});
					ctx = _ctx;
				}
				else {
					ctx = it->second;
				}

				ctx_update_for_watch(ctx, flag, fd, fn, err);
				TRACE_IOE("[observer_abstract][#%d]watch_ioe: update: %d, now: %d", fd, flag, ctx->flag);
			}

			inline void unwatch_ioe( u8_t const& flag, int const& fd) {

				WAWO_ASSERT(fd>0);
				WAWO_ASSERT(flag > 0 && flag <= 0xFF);

				observer_ctx_map::iterator it = m_ctxs.find(fd); 
				
				if (it == m_ctxs.end()) return;
				WWRP<observer_ctx> ctx = it->second ;

				ctx_update_for_unwatch(ctx, flag, fd);

				if ((ctx->flag == 0)) {
					TRACE_IOE("[select][#%d]erase fd from IOE", ctx->fd);
					m_ctxs.erase(it);
				}
			}

			void watch(u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err )
			{
				WAWO_ASSERT( flag > 0 );
				if( 0 != flag ) {
					WAWO_CONDITION_CHECK( m_ctxs.size() < WAWO_SELECT_LIMIT ) ;
					watch_ioe(flag, fd, fn, err);
				}
			}

			void unwatch(u8_t const& flag, int const& fd)
			{
				WAWO_ASSERT( flag > 0 );
				if( 0 != flag ) {
					unwatch_ioe( flag, fd );
				}
			}

	public:
		void check_ioe() {
			int fd_added_count = 0;
			int idx = 0;

			observer_ctx_map::iterator it = m_ctxs.begin();
			while( it != m_ctxs.end() ) {

				WWRP<observer_ctx> const& ctx = it->second ;
				{
					unique_lock<spin_mutex> ulock(ctx->r_mutex, wawo::thread::try_to_lock);
					bool lock_ok = ulock.own_lock();

					if ( lock_ok && (ctx->flag&IOE_READ) && (ctx->r_state == S_READ_IDLE)) {

						if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
							++idx;
							WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
							fd_added_count = 0;
							m_ctxs_to_check[idx]->max_fd_v = 0;
						}

						FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_r);
						m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

						if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
							m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
						}
						++fd_added_count;
					}
				}

				{
					unique_lock<spin_mutex> ulock(ctx->w_mutex, wawo::thread::try_to_lock);
					bool lock_ok = ulock.own_lock();
					if ( lock_ok && (ctx->flag&IOE_WRITE) && (ctx->fn_info[IOE_SLOT_WRITE].fn != NULL) && (ctx->w_state == S_WRITE_IDLE)) {

						if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
							++idx;
							WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
							fd_added_count = 0;
							m_ctxs_to_check[idx]->max_fd_v = 0;
						}

						FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_w);
						m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

						if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
							m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
						}

						++fd_added_count;
					}
				
					if (lock_ok && (ctx->flag&IOE_WRITE) && (ctx->fn_info[IOE_SLOT_WRITE].err != NULL) && (ctx->w_state == S_WRITE_IDLE)) {
						//for async connect
						if (fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT) {
							++idx;
							WAWO_ASSERT(idx < WAWO_SELECT_BUCKET_MAX);
							fd_added_count = 0;
							m_ctxs_to_check[idx]->max_fd_v = 0;
						}

						FD_SET((ctx->fd), &m_ctxs_to_check[idx]->fds_ex);
						m_ctxs_to_check[idx]->ctxs[m_ctxs_to_check[idx]->count++] = ctx;

						if (ctx->fd > m_ctxs_to_check[idx]->max_fd_v) {
							m_ctxs_to_check[idx]->max_fd_v = ctx->fd;
						}
						++fd_added_count;
					}
				}

				++it;
			}

			for( int j=0;j<=idx;++j ) {
				if( m_ctxs_to_check[j]->count > 0 ) {
					_select_sockets( m_ctxs_to_check[j] );
					m_ctxs_to_check[j]->reset();
				}
			}
		}

	public:
		void init() {
			observer_abstract::init();

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				m_ctxs_to_check[i] = new ctxs_to_check();
				WAWO_ALLOC_CHECK( m_ctxs_to_check[i] , sizeof(ctxs_to_check) );
			}
		}
		void deinit() {
			observer_abstract::deinit();
			observer_abstract::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			WAWO_ASSERT(m_ctxs.size() == 0);

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				m_ctxs_to_check[i]->reset() ;
			}

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;++i ) {
				WAWO_DELETE( m_ctxs_to_check[i]) ;
			}
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
				WWRP<observer_ctx>& ctx = sockets->ctxs[c_idx] ;

				int const& fd = ctx->fd;
				if( FD_ISSET( fd, &fds_r ) ) {
					FD_CLR(fd, &fds_r);
					--ready_c ;

					if ( WAWO_LIKELY(ctx->flag&IOE_INFINITE_WATCH_READ)) {
						{
							lock_guard<spin_mutex> lg_ctx(ctx->r_mutex);
							ctx->r_state = S_READ_POSTED;
						}
						WWRP<wawo::task::task> _t = wawo::make_ref<io_select_task>(ctx->fn_info[IOE_SLOT_READ].fn, ctx, static_cast<u8_t>(IOE_READ));
						_t->run();
					}
					else {

						WWRP<observer_ctx> _ctx = wawo::make_ref<observer_ctx>();
						_ctx->fd = ctx->fd;
						_ctx->fn_info[IOE_SLOT_READ].fn = ctx->fn_info[IOE_SLOT_READ].fn;
						_ctx->poll_type = ctx->poll_type;
						_ctx->flag = ctx->flag;
						_ctx->r_state = S_READ_POSTED;
						unwatch(IOE_READ, ctx->fd);

						WWRP<wawo::task::task> _t = wawo::make_ref<io_select_task>( _ctx->fn_info[IOE_SLOT_READ].fn, _ctx, static_cast<u8_t>(IOE_READ));
						_t->run();
					}
				}

				if( FD_ISSET( fd, &fds_w ) ) {
					FD_CLR(fd, &fds_w);
					--ready_c;

					if ( WAWO_LIKELY(ctx->flag&IOE_INFINITE_WATCH_WRITE)) {
						{
							lock_guard<spin_mutex> lg_ctx(ctx->w_mutex);
							ctx->w_state = S_WRITE_POSTED;
						}

						WWRP<wawo::task::task> _t = wawo::make_ref<io_select_task>(ctx->fn_info[IOE_SLOT_WRITE].fn, ctx, static_cast<u8_t>(IOE_WRITE));
						_t->run();
					}
					else {
						WWRP<observer_ctx> _ctx = wawo::make_ref<observer_ctx>();
						_ctx->fd = ctx->fd;
						_ctx->fn_info[IOE_SLOT_WRITE].fn = ctx->fn_info[IOE_SLOT_WRITE].fn;
						_ctx->poll_type = ctx->poll_type;
						_ctx->flag = ctx->flag;
						_ctx->w_state = S_WRITE_POSTED;

						unwatch(IOE_WRITE, ctx->fd);

						WWRP<wawo::task::task> _t = wawo::make_ref<io_select_task>(_ctx->fn_info[IOE_SLOT_WRITE].fn, _ctx, static_cast<u8_t>(IOE_WRITE));
						_t->run();
					}
				}

				if( FD_ISSET(fd, &fds_ex) ) {
					FD_CLR(fd, &fds_ex);
					--ready_c ;

					WAWO_ASSERT(ctx->fd > 0);

					fn_io_event_error fn = ctx->fn_info[IOE_SLOT_WRITE].err;

					unwatch(IOE_READ, ctx->fd);
					unwatch(IOE_WRITE, ctx->fd);//only trigger once

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt == -1) {
						ec = wawo::socket_get_last_errno();
					}
					ec = WAWO_NEGATIVE(ec);
					WAWO_ASSERT(fn != NULL);
					WAWO_ASSERT(ec != wawo::OK);
					fn(ec);
				}
			}
		}
	};
}}}
#endif//