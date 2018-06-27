#ifndef _WAWO_NET_POLLER_IMPL_IOCP_HPP
#define _WAWO_NET_POLLER_IMPL_IOCP_HPP

#include <wawo/core.hpp>
#include <wawo/net/poller_abstract.hpp>

#include <wawo/net/socket.hpp>
#include <mswsock.h>

namespace wawo { namespace net { namespace impl {

	class iocp :
		public poller_abstract
	{
		HANDLE m_handle;
	public:
		iocp():
			poller_abstract(),
			m_handle(NULL)
		{}

		void init() {
			poller_abstract::init();

			WAWO_ASSERT(m_handle == NULL);
			m_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (u_long) 0, 0);
			WAWO_ALLOC_CHECK(m_handle, sizeof(m_handle));
		}

		void deinit() {
			WAWO_ASSERT(m_handle != NULL);
			::CloseHandle(m_handle);
			m_handle = NULL;

			poller_abstract::deinit();
		}

		void interrupt_wait() {
			WAWO_ASSERT(m_handle != NULL);
			BOOL postrt = PostQueuedCompletionStatus(m_handle, -1,(DWORD) NULL,0 );
			if (postrt == FALSE ) {
				WAWO_ERR("PostQueuedCompletionStatus failed: %d", wawo::socket_get_last_errno());
				return;
			}
		}

	public:
		void do_poll() {

			WAWO_ASSERT(m_handle > 0);
			DWORD len;
			LPOVERLAPPED lpol;
			int ii;
			int ec = 0;

			DWORD dwWaitMill = _before_wait();
			if (dwWaitMill == -1) {
				dwWaitMill = INFINITE;
			}
			bool bLastWait = (dwWaitMill != 0);
			BOOL getOk = GetQueuedCompletionStatus(m_handle, &len, (LPDWORD)&ii, (LPOVERLAPPED*)&lpol, dwWaitMill);
			if (bLastWait) {
				_after_wait();
			}
			
			if (WAWO_UNLIKELY(getOk==0)) {
				ec = wawo::socket_get_last_errno();
				if (ec == WAIT_TIMEOUT ) {
					WAWO_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec );
					return;
				}
				WAWO_INFO("[iocp]GetQueuedCompletionStatus failed, %d", wawo::socket_get_last_errno());
			}

			if (len == -1) {
				WAWO_INFO("[iocp]GetQueuedCompletionStatus waken signal");
				return;
			}
			WAWO_ASSERT(lpol != NULL);

			iocp_overlapped_ctx* ctx = CONTAINING_RECORD(lpol, iocp_overlapped_ctx, _overlapped);
			WAWO_ASSERT(ctx != NULL);
			WAWO_ASSERT(ctx->so != NULL);
			WWRP<socket> so = ctx->so;
			ctx->len = len;
			ctx->ec = ec;

			switch (ctx->action) {
				case IOCP_ACTION_ACCEPT:
				{
					WAWO_ASSERT(ctx->ref_so != NULL);
					ctx->ref_so->iocp_accepted(so);
					ctx->ref_so->end_read();
					ctx->ref_so->begin_read();

					so->iocp_unmake_ctx(IOCP_OVERLAPPED_CTX_ACCEPT);
					so->iocp_accept_done();
					return;
				}
				break;
				case IOCP_ACTION_READ:
				{
					WAWO_ASSERT(ctx->ref_so == NULL);
					WAWO_ASSERT(ctx->so != NULL);
					so->iocp_read_done();
				}
				break;
				case IOCP_ACTION_WRITE:
				{
					WAWO_ASSERT(ctx->ref_so == NULL);
					WAWO_ASSERT(ctx->so != NULL);
					ctx->action = IOCP_ACTION_IDLE;

					so->iocp_write_done();
				}
				break;
				default:
				{
					WAWO_THROW("unknown io event flag");
				}
			}
		}

		void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn ) {
			WWRP<socket> so = wawo::dynamic_pointer_cast<socket>(fnctx);
			WAWO_ASSERT(so != NULL);

			if (flag&IOE_IOCP_BIND) {
				WAWO_DEBUG("[#%d][CreateIoCompletionPort] add", so->fd() );
				so->iocp_init();
				HANDLE new_so_cp = ::CreateIoCompletionPort((HANDLE)so->fd(), m_handle, (u_long)0, 0);
				WAWO_ASSERT(new_so_cp == m_handle);
				return;
			}

			if (flag&IOE_LISTEN) {
				WAWO_ASSERT(so->is_listener());
				WAWO_ASSERT(m_handle != NULL);
				HANDLE listencpport = ::CreateIoCompletionPort((HANDLE)so->fd(), m_handle, (u_long)0, 0);
				WAWO_ASSERT(listencpport == m_handle);
				return;
			}

			if (flag&IOE_READ) {
				if (so->is_listener()) {
					//listen
					LPFN_ACCEPTEX lpfnAcceptEx = NULL;
					GUID GuidAcceptEx = WSAID_ACCEPTEX;
					DWORD dwbytes;

					int iResult = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
									&GuidAcceptEx, sizeof(GuidAcceptEx),
									&lpfnAcceptEx, sizeof(lpfnAcceptEx),
									&dwbytes, NULL, NULL);

					if (iResult == SOCKET_ERROR) {
						err(wawo::socket_get_last_errno(), fnctx );
						return;
					}

					WWRP<socket> _so = wawo::make_ref<socket>(so->sock_family(), so->sock_type(), so->sock_protocol() );
					int openrt = _so->open();

					if (openrt != wawo::OK) {
						err(openrt, fnctx);
						return;
					}
					_so->iocp_make_ctx(IOCP_OVERLAPPED_CTX_ACCEPT);
					WWSP<iocp_overlapped_ctx>& ctx = _so->iocp_ctx(IOCP_OVERLAPPED_CTX_ACCEPT);
					ctx->ref_so = so;
					ctx->action = IOCP_ACTION_ACCEPT;

					BOOL acceptrt = lpfnAcceptEx(fd, _so->fd(),
						ctx->buf, 0,
						sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
						NULL, &(ctx->_overlapped) );

					if ( !acceptrt)
					{
						int ec = wawo::socket_get_last_errno();
						if (ec != ERROR_IO_PENDING) {
							_so->ch_close();
							err(ec, fnctx);
							return;
						}
					}
				}
				else {
					//read begin
					WAWO_ASSERT(so != NULL);
					so->iocp_reset_ctx(IOCP_OVERLAPPED_CTX_READ);
					WWSP<iocp_overlapped_ctx>& _ctx = so->iocp_ctx(IOCP_OVERLAPPED_CTX_READ);
					static DWORD flags = 0;
					if (::WSARecv(so->fd(), &_ctx->wsabuf, 1, NULL, &flags, &_ctx->_overlapped, NULL) == SOCKET_ERROR) {
						//int ec = wawo::socket_get_last_errno();
						//if (ec != ERROR_IO_PENDING) {
						//	if (_ctx->action == IOCP_ACTION_IDLE) {
						//		err(ec, fnctx);
						//		return;
						//	}
						//}
					}
					_ctx->action = IOCP_ACTION_READ;
				}
			}
		}

		void do_unwatch(u8_t const& flag, int const& fd)
		{
		
		}
	};
}}}
#endif