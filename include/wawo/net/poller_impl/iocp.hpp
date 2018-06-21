#ifndef _WAWO_NET_POLLER_IMPL_IOCP_HPP
#define _WAWO_NET_POLLER_IMPL_IOCP_HPP

#include <wawo/core.hpp>
#include <wawo/net/io_event_loop.hpp>

#include <wawo/net/socket.hpp>
#include <mswsock.h>

#define WAWO_IOCP_BUFFER_SIZE 1024*64
namespace wawo { namespace net { namespace impl {

	class iocp :
		public io_event_loop
	{
		struct iocp_overlapped_ctx {
			WSAOVERLAPPED* _overlapped;
			int fd;
			WWRP<socket> so;
			int flag;
			WSABUF wsabuf;
			char buf[WAWO_IOCP_BUFFER_SIZE];
			DWORD buflen;
		};

		struct iocp_ctx {
			iocp_overlapped_ctx accept;
			iocp_overlapped_ctx read;
			iocp_overlapped_ctx write;
		};

		typedef std::map<int, iocp_overlapped_ctx> iocp_ctx_map;

		HANDLE m_handle;
		iocp_ctx_map m_ctx_map;

	public:
		iocp():
			io_event_loop(),
			m_handle(NULL)
		{}

		void init() {
			WAWO_ASSERT(m_handle == NULL);
			m_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (u_long) 0, 0);
			WAWO_ALLOC_CHECK(m_handle, sizeof(m_handle));
		}

		void deinit() {
			WAWO_ASSERT(m_handle != NULL);
			CloseHandle(m_handle);
			m_handle = NULL;
		}

	public:
		void do_poll() {

			WAWO_ASSERT(m_handle > 0);
			DWORD len;
			WSAOVERLAPPED* ov;
			int ii;
			DWORD dwWaitMill = INFINITE;
			bool bLastWait = false;
			{
				//for wait time
				lock_guard<spin_mutex> exector_lg(m_tq_mtx);
				if (m_tq_standby->size()) {
					dwWaitMill = 0;
				} else {
					m_in_wait = true;
					bLastWait = true;
				}
			}
			BOOL getOk = GetQueuedCompletionStatus(m_handle, &len, (LPDWORD)&ii, &ov, dwWaitMill);
			if (bLastWait) {
				lock_guard<spin_mutex> exector_lg(m_tq_mtx);
				{
					m_in_wait = false;
				}
			}
			if (!getOk) {
				WAWO_ERR("[iocp]GetQueuedCompletionStatus failed, %d", wawo::socket_get_last_errno());
				return;
			}
		}

		void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn, fn_io_event_error const& err, WWRP<ref_base> const& fnctx ) {
			WWRP<socket> so = wawo::dynamic_pointer_cast<socket>(fnctx);
			WAWO_ASSERT(so != NULL);
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

					WWSP<iocp_overlapped_ctx> _iocp_overlapped_ctx = wawo::make_shared<iocp_overlapped_ctx>();
					WSAOVERLAPPED* _wsaoverlapped = (WSAOVERLAPPED*)::malloc(sizeof(WSAOVERLAPPED));
					memset(&_wsaoverlapped, 0, sizeof(_wsaoverlapped));

					_iocp_overlapped_ctx->_overlapped = _wsaoverlapped;
					_iocp_overlapped_ctx->fd = _so->fd();
					_iocp_overlapped_ctx->flag = IOE_READ;
					_iocp_overlapped_ctx->buflen = 0;

					memset(_iocp_overlapped_ctx->buf, 0, WAWO_IOCP_BUFFER_SIZE);
					int acceptrt = lpfnAcceptEx(fd, _so->fd(),
						_iocp_overlapped_ctx->buf, WAWO_IOCP_BUFFER_SIZE - ((sizeof(sockaddr_in)+16)*2),
						sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
						&_iocp_overlapped_ctx->buflen, _wsaoverlapped);

					if (acceptrt != wawo::OK)
					{
						_so->ch_close();
						err(acceptrt, fnctx);
						return;
					}

					HANDLE port2 = CreateIoCompletionPort( (HANDLE)_so->fd(), m_handle, (u_long)0, 0 );
					if (port2 == NULL) {
						_so->ch_close();
						err(wawo::socket_get_last_errno(), fnctx);
						return;
					}

					WAWO_ASSERT(port2 == m_handle);
				}
			}
		}

		void do_unwatch(u8_t const& flag, int const& fd)
		{
		
		}
	};
}}}
#endif