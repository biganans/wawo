#ifndef _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_
#define _WAWO_NET_OBSERVER_IMPL_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.hpp>
#include <wawo/net/poller_abstract.hpp>
#include <wawo/net/socket_api.hpp>

namespace wawo { namespace net { namespace impl {

	class select:
		public poller_abstract
	{
		fd_set m_fds_r;
		fd_set m_fds_w;
		fd_set m_fds_e;

		SOCKET m_signalfds[2];
		public:
			select():
				poller_abstract()
			{
				m_signalfds[0] = wawo::E_INVALID_SOCKET;
				m_signalfds[1] = wawo::E_INVALID_SOCKET;
			}

			~select() {}

			inline void watch_ioe( u8_t const& flag, SOCKET const& fd, fn_io_event const& fn ) {

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

				ctx_update_for_watch(ctx, flag, fn );
				TRACE_IOE("[io_event_loop][#%d]watch_ioe: update: %d, now: %d", fd, flag, ctx->flag);
			}

			inline void unwatch_ioe( u8_t const& flag, SOCKET const& fd) {

				WAWO_ASSERT(fd>0);
				WAWO_ASSERT(flag > 0 && flag <= 0xFF);

				poller_ctx_map::iterator it = m_ctxs.find(fd); 
				
				if (it == m_ctxs.end()) return;
				WWRP<poller_ctx> ctx = it->second ;

				ctx_update_for_unwatch(ctx, flag);

				if ((ctx->flag == 0)) {
					TRACE_IOE("[select][#%d]erase fd from IOE", ctx->fd);
					m_ctxs.erase(it);
				}
			}

			void do_watch(u8_t const& flag, SOCKET const& fd, fn_io_event const& fn)
			{
				WAWO_ASSERT( flag > 0 );
				if (m_ctxs.size() >= FD_SETSIZE) {
					fn({ flag&IOE_READ ? AIO_READ : AIO_WRITE,fd, wawo::E_POLLER_FD_COUNT_LIMIT, 0 });
					return;
				}
				watch_ioe(flag, fd, fn);
			}

			void do_unwatch(u8_t const& flag, SOCKET const& fd)
			{
				WAWO_ASSERT( flag > 0 );
				unwatch_ioe( flag, fd );
			}

#pragma warning(push)
#pragma warning(disable:4389)
		void do_poll() {

			FD_ZERO(&m_fds_r);
			FD_ZERO(&m_fds_w);
			FD_ZERO(&m_fds_e);

			SOCKET max_fd_v = wawo::E_INVALID_SOCKET;

			poller_ctx_map::iterator it = m_ctxs.begin();
			while( it != m_ctxs.end() ) {
				WWRP<poller_ctx> const& ctx = it->second ;
				if(ctx->flag&IOE_READ) {
					//TRACE_IOE("[select]check read evt for: %d", ctx->fd );
					FD_SET((ctx->fd), &m_fds_r);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}

				if(ctx->flag&IOE_WRITE && ctx->fn[IOE_SLOT_WRITE] != NULL ) {
					FD_SET(ctx->fd, &m_fds_w);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
					FD_SET((ctx->fd), &m_fds_e);
					if (ctx->fd > max_fd_v) {
						max_fd_v = ctx->fd;
					}
				}
				
				++it;
			}
			_select(max_fd_v, m_fds_r, m_fds_w,m_fds_e );
		}

		inline void _select(SOCKET const& max_fd_v, fd_set& fds_r, fd_set& fds_w, fd_set& fds_ex) {
			timeval _tv = { 0,0 };
			timeval* tv;
			const long long wait_in_nano = _before_wait();
			if (wait_in_nano == ~0) {
				tv = 0;//0 for infinite
			} else {
				tv = &_tv;
				if (wait_in_nano > 0) {
					_tv.tv_sec = static_cast<long>(wait_in_nano / 1000000000ULL);
					_tv.tv_usec = static_cast<long>(wait_in_nano % 1000000000ULL)/1000;
				}
			}
			const bool bLastWait = (wait_in_nano != 0);
			int ready_c = ::select((int)max_fd_v, &fds_r, &fds_w, &fds_ex, tv); //only read now
			if (bLastWait) {
				_after_wait();
			}
			if (ready_c == 0) {
				return;
			}
			else if (ready_c == -1) {
				//notice 10038
				WAWO_ERR("[SELECT]select error, errno: %d", socket_get_last_errno());
				return;
			}

			//check begin
			//int total_rc = 0;

			poller_ctx_map::iterator it = m_ctxs.begin();
			while (it != m_ctxs.end() && (ready_c > 0)) {
				poller_ctx_map::iterator it_cur = it;
				++it;

				WWRP<poller_ctx> ctx = it_cur->second;
				WAWO_ASSERT(ctx != NULL);

				SOCKET const& fd = ctx->fd;
				if (FD_ISSET(fd, &fds_r)) {
					FD_CLR(fd, &fds_r);
					--ready_c;
					fn_io_event fn = ctx->fn[IOE_SLOT_READ];
					WAWO_ASSERT(fn != nullptr);
					fn({ AIO_READ, 0,0 });
					//++total_rc;
				}

				if (FD_ISSET(fd, &fds_w)) {
					FD_CLR(fd, &fds_w);
					--ready_c;
					ctx->fn[IOE_SLOT_WRITE]({ AIO_WRITE,0,0 });
				}

				if (FD_ISSET(fd, &fds_ex)) {
					FD_CLR(fd, &fds_ex);
					--ready_c;
					WAWO_ASSERT(ctx->fd > 0);

					int ec;
					socklen_t optlen = sizeof(int);
					int getrt = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&ec, &optlen);
					if (getrt == -1) {
						ec = wawo::socket_get_last_errno();
					} else {
						ec = WAWO_NEGATIVE(ec);
					}
					WAWO_ASSERT(ec != wawo::OK);

					if (ctx->fn[IOE_SLOT_READ] != nullptr) { 
						ctx->fn[IOE_SLOT_READ]({ AIO_READ, ctx->fd, ec,0 });
					}
					if (ctx->fn[IOE_SLOT_WRITE] != nullptr) {
						ctx->fn[IOE_SLOT_WRITE]({ AIO_WRITE,ctx->fd, ec,0 });
					}
				}
			}
		}

#pragma warning(pop)

		void init() {
			poller_abstract::init();

			int rt = wawo::net::socket_api::posix::socketpair(F_AF_INET, T_STREAM, P_TCP, m_signalfds);
			WAWO_ASSERT(rt == wawo::OK);

			rt = wawo::net::socket_api::helper::turnon_nonblocking(m_signalfds[0]);
			WAWO_ASSERT(rt==wawo::OK);

			rt = wawo::net::socket_api::helper::turnon_nonblocking(m_signalfds[1]);
			WAWO_ASSERT(rt == wawo::OK);

			rt = wawo::net::socket_api::helper::turnon_nodelay(m_signalfds[1]);
			WAWO_ASSERT(rt == wawo::OK);

			watch_ioe(IOE_READ, m_signalfds[0], [fd= m_signalfds[0]](async_io_result const& r) {
				//do nothing...JUST FOR API compatible
				(void)r;
				WAWO_DEBUG("[select]interrupt wait callback");
				WAWO_ASSERT(r.v.code == wawo::OK);
				byte_t tmp[1];
				int ec = wawo::OK;
				u32_t c = wawo::net::socket_api::posix::recv(fd, tmp, 1, ec, 0);
				WAWO_ASSERT(ec == wawo::OK);
				WAWO_ASSERT(tmp[0] == 'i');
				WAWO_ASSERT(c == 1);
			});
		}
		void deinit() {
			unwatch_ioe(IOE_READ, m_signalfds[0]);
			poller_abstract::ctxs_cancel_all(m_ctxs);
			WAWO_CLOSE_SOCKET(m_signalfds[0]);
			WAWO_CLOSE_SOCKET(m_signalfds[1]);
			m_signalfds[0] = wawo::E_INVALID_SOCKET;
			m_signalfds[1] = wawo::E_INVALID_SOCKET;
			poller_abstract::deinit();
		}
		void interrupt_wait() {
			WAWO_ASSERT(m_signalfds[0] > 0);
			WAWO_ASSERT(m_signalfds[1] > 0);
			int ec;
			const static byte_t interrutp_a[1] = { (byte_t) 'i' };
			u32_t c = wawo::net::socket_api::posix::send(m_signalfds[1], interrutp_a, 1, ec, 0 );
			WAWO_ASSERT(c == 1);
			WAWO_ASSERT(ec == 0);
		}
	};
}}}
#endif//