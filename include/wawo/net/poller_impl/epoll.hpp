#ifndef _WAWO_NET_OBSERVER_IMPL_EPOLL_HPP_
#define _WAWO_NET_OBSERVER_IMPL_EPOLL_HPP_

#include <sys/epoll.h>

#include <wawo/core.hpp>
#include <wawo/net/io_event_loop.hpp>

#include <syscall.h>
#include <poll.h>

namespace wawo { namespace net { namespace impl {

	

	class epoll:
		public io_event_loop
	{
		int m_epfd;

	public:
		epoll():
			io_event_loop(),
			m_epfd(-1)
		{
		}

		~epoll() {
			WAWO_ASSERT( m_epfd == -1 );
		}

		void watch( u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err ) {

			WAWO_ASSERT(fd > 0);
			WAWO_ASSERT(cookie != NULL);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT(err != NULL);

			//u8_t fflag = flag;
			WAWO_ASSERT( (flag&(IOE_READ|IOE_WRITE)) != 0 );

			u16_t epoll_op ;

			WWRP<poller_ctx> ctx;
			typename poller_ctx_map::iterator it = m_ctxs.find(fd);

			if( it == m_ctxs.end() ) {
				epoll_op = EPOLL_CTL_ADD;

				WWRP<poller_ctx> _ctx = wawo::make_ref<poller_ctx>();
				_ctx->fd = fd;
				_ctx->poll_type = T_EPOLL;
				_ctx->flag = 0;

				ctx = _ctx;
			} else {
				ctx = it->second ;
				epoll_op = EPOLL_CTL_MOD;
			}

			struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default
			epEvent.data.ptr = (void*)ctx.get();
			epEvent.events = EPOLLPRI;

#ifdef WAWO_IO_MODE_EPOLL_USE_ET
			epEvent.events |= EPOLLET;
#else
			epEvent.events |= EPOLLLT;
#endif
			if (flag&IOE_READ) {
				epEvent.events |= EPOLLIN;
			}
			if (flag&IOE_WRITE) {
				epEvent.events |= EPOLLOUT;
			}

			WAWO_ASSERT(epoll_op == EPOLL_CTL_ADD || epoll_op == EPOLL_CTL_MOD);
			WAWO_ASSERT(m_epfd > 0);
			int rt = epoll_ctl(m_epfd, epoll_op, fd, &epEvent);

			if (rt == -1) {
				wawo::task::fn_task_void _lambda = [err, rt, cookie]() -> void {
					err(rt, cookie);
				};
				WAWO_SCHEDULER->schedule(_lambda);
				WAWO_ERR("[EPOLL][##%d][#%d][watch]epoll op failed, op code: %d, op flag: %d, error_code: %d, schedule error", m_epfd, fd, epoll_op, flag, socket_get_last_errno());
				return;
			}

			WAWO_ASSERT((ctx->flag&flag) == 0);
			ctx_update_for_watch(ctx, flag, fd, cookie, fn, err );

			if (epoll_op == EPOLL_CTL_ADD) {
				TRACE_IOE("[EPOLL][##%d][#%d][watch]epoll op success, op code: %d, op flag: %d, EPOLL_CTL_ADD, add to ctxs", m_epfd, fd, epoll_op, flag);
				m_ctxs.insert({ fd,ctx });
			}
			TRACE_IOE("[EPOLL][##%d][#%d][watch]epoll op success, op code: %d, op flag: %d, new flag: %d", m_epfd, fd, epoll_op, flag, ctx->flag);
		}

		void unwatch( u8_t const& flag, int const& fd ) {

			WAWO_ASSERT(fd > 0);
			WAWO_ASSERT( m_epfd > 0 );

			poller_ctx_map::iterator it = m_ctxs.find(fd);

			if( it == m_ctxs.end() ) { return ; }

			WWRP<poller_ctx> ctx = it->second;

			u16_t epoll_op = ( !((ctx->flag&(IOE_READ|IOE_WRITE))&(~flag)) ) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

			struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default

			if( epoll_op == EPOLL_CTL_MOD ) {
				epEvent.data.ptr = (void*) ctx.get();
				epEvent.events = EPOLLPRI ;
	#ifdef WAWO_IO_MODE_EPOLL_USE_ET
				epEvent.events |= EPOLLET;
	#else
				epEvent.events |= EPOLLLT;
	#endif
				if( ctx->flag&IOE_READ ) {
					epEvent.events |= EPOLLIN;
				}
				if( ctx->flag&IOE_WRITE ) {
					epEvent.events |= EPOLLOUT;
				}

				if( flag&IOE_READ ) {
					epEvent.events &= ~EPOLLIN;
				}
				if( flag&IOE_WRITE ) {
					epEvent.events &= ~EPOLLOUT;
				}
				WAWO_ASSERT( (epEvent.events&EPOLLIN) || (epEvent.events&EPOLLOUT) );
			}

			int nRet = epoll_ctl(m_epfd,epoll_op,fd,&epEvent) ;
			if( nRet == -1 ) {
				int ec = WAWO_NEGATIVE(wawo::socket_get_last_errno());
				WAWO_WARN("[EPOLL][##%d][#%d][unwatch]epoll op failed, op code: %d, op flag: %d, error_code: %d, erase from ctxs", m_epfd, fd, epoll_op, flag, ec);
				m_ctxs.erase(it);
				return;
			}

			ctx_update_for_unwatch(ctx, flag, fd);
			if( epoll_op == EPOLL_CTL_DEL ) {
				WAWO_ASSERT(ctx->flag == 0 );
				TRACE_IOE("[EPOLL][##%d][#%d][unwatch]epoll op success, op code: %d, op flag: %d, EPOLL_CTL_DEL, erase from ctxs", m_epfd, fd, epoll_op, flag );
				m_ctxs.erase(it);
			}
			TRACE_IOE("[EPOLL][##%d][#%d][unwatch]epoll op success, op code: %d, op flag: %d, new flag: %d", m_epfd, fd, epoll_op, flag, ctx->flag );
		}

	public:
		void init() {
			//io_event_loop::init();

			//the size argument is ignored since Linux 2.6.8, but must be greater than zero
			m_epfd = epoll_create( WAWO_EPOLL_CREATE_HINT_SIZE );
			if( -1 == m_epfd ) {
				WAWO_ERR("[EPOLL]epoll create failed!, errno: %d", socket_get_last_errno() );
				WAWO_THROW("create epoll handle failed");
			}

			WAWO_DEBUG("[EPOLL]init write epoll handle ok" );
		}
		void deinit() {
			WAWO_DEBUG( "[EPOLL]deinit ..." ) ;
			io_event_loop::deinit();

			io_event_loop::ctxs_cancel_all(m_ctxs);
			m_ctxs.clear();

			WAWO_CONDITION_CHECK( m_epfd != -1);

			int rt = ::close( m_epfd );
			if( -1 == rt ) {
				WAWO_ERR("[EPOLL]epoll handle: %d, epoll close failed!: %d", m_epfd, socket_get_last_errno() );
				WAWO_THROW("EPOLL::deinit epoll handle failed");
			} else {
				TRACE_IOE("[EPOLL]deinit epoll handle ok" );
			}
			m_epfd = -1;
			TRACE_IOE("[EPOLL] EPOLL::deinit() done" ) ;
		}

		void do_poll() {
			WAWO_ASSERT( m_epfd > 0 );

			struct epoll_event epEvents[WAWO_EPOLL_PER_HANDLE_SIZE] ;
			int nTotalEvents = epoll_wait(m_epfd, epEvents,WAWO_EPOLL_PER_HANDLE_SIZE,0);

			if ( -1 == nTotalEvents ) {
				WAWO_ERR("[EPOLL]epoll wait event failed!, errno: %d", m_epfd, socket_get_last_errno() );
				return ;
			}

			for( int i=0;i<nTotalEvents;++i) {

				WAWO_ASSERT( epEvents[i].data.ptr != NULL );
				WWRP<poller_ctx> ctx (static_cast<poller_ctx*> (epEvents[i].data.ptr)) ;
				WAWO_ASSERT(ctx->fd > 0);

				wawo::u32_t events = ((epEvents[i].events) & 0xFFFFFFFF) ;

				TRACE_IOE( "[EPOLL][##%d][#%d]EVT: events(%d)", m_epfd, ctx->fd, events );

				if( events&(EPOLLERR|EPOLLHUP) ) {
					//TRACE_IOE( "[EPOLL][##%d][#%d]EVT: (EPOLLERR|EPOLLHUB), post IOE_ERROR", m_epfd, ctx->fd );

					RE _re;

					if (events&EPOLLIN) {
						events &= ~EPOLLIN;

						WAWO_ASSERT(ctx->fn[IOE_SLOT_READ].fn != NULL);
						WAWO_ASSERT(ctx->fn[IOE_SLOT_READ].cookie != NULL);

						_re.read.fn = ctx->fn[IOE_SLOT_READ].fn;
						_re.read.cookie = ctx->fn[IOE_SLOT_READ].cookie;
					}

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt == -1) {
						ec = wawo::socket_get_last_errno();
					}
					ec = WAWO_NEGATIVE(ec);
					if( WAWO_UNLIKELY(ec == wawo::OK)) {
						//socket recv/send ing,,, new evt comes,,we may get ec==wawo::OK
						ec = wawo::E_UNKNOWN;
					}

					WAWO_ASSERT( (events&EPOLLERR) ? ec != wawo::OK: true);

					_re.rd_err.fn = ctx->fn[IOE_SLOT_READ].err;
					_re.rd_err.cookie = ctx->fn[IOE_SLOT_READ].cookie;

					_re.wr_err.fn = ctx->fn[IOE_SLOT_WRITE].err;
					_re.wr_err.cookie = ctx->fn[IOE_SLOT_WRITE].cookie;

					unwatch(IOE_WRITE, ctx->fd);
					unwatch(IOE_READ, ctx->fd);

					wawo::task::fn_task_void _lambda = [_re,ec]() -> void {
						if (_re.read.fn != NULL) {
							_re.read.fn(_re.read.cookie);
						}
						if (_re.rd_err.fn != NULL) {
							_re.rd_err.fn(ec,_re.rd_err.cookie);
						}
						if (_re.wr_err.fn != NULL) {
							_re.wr_err.fn(ec, _re.wr_err.cookie);
						}
					};

					events &= ~(EPOLLERR | EPOLLHUP);

					WAWO_SCHEDULER->schedule(_lambda);
					continue;
				}

				if (events&EPOLLIN) {
					//TRACE_IOE("[EPOLL][##%d][#%d]EVT: EPOLLIN", m_epfd, ctx->fd);
					events &= ~EPOLLIN;

					fn_io_event& fn = ctx->fn[IOE_SLOT_READ].fn;
					WWRP<ref_base>& cookie = ctx->fn[IOE_SLOT_READ].cookie;
					WAWO_ASSERT(fn != NULL);
					WAWO_ASSERT(cookie != NULL);

					WWRP<wawo::task::task> _t = wawo::make_ref<wawo::task::task>(fn, cookie);

					if ( WAWO_LIKELY( !(ctx->flag&IOE_INFINITE_WATCH_READ)) ) {
						unwatch(IOE_READ, ctx->fd);
					}

					WAWO_SCHEDULER->schedule(_t);
				}

				if (events & EPOLLOUT) {
					//TRACE_IOE("[EPOLL][##%d][#%d]EVT: EPOLLOUT", m_epfd, ctx->fd);
					events &= ~EPOLLOUT;

					fn_io_event& fn = ctx->fn[IOE_SLOT_WRITE].fn;
					WWRP<ref_base>& cookie = ctx->fn[IOE_SLOT_WRITE].cookie;
					WAWO_ASSERT(fn != NULL);
					WAWO_ASSERT(cookie != NULL );

					WWRP<wawo::task::task> _t = wawo::make_ref<wawo::task::task>(fn, cookie);

					if (WAWO_LIKELY( !(ctx->flag&IOE_INFINITE_WATCH_WRITE))) {
						unwatch(IOE_WRITE, ctx->fd);
					}

					WAWO_SCHEDULER->schedule(_t);
				}

				if (events&EPOLLPRI) {
					WAWO_ERR("[EPOLL][##%d][#%d]EVT: EPOLLPRI", m_epfd, ctx->fd);
					WAWO_THROW("EPOLLPRI arrive!!!");
					events &= ~EPOLLPRI;
				}

				WAWO_ASSERT( events == 0 );
			}
		}
	};

}}}
#endif
