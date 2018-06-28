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

#define WAWO_IOCP_BUFFER_SIZE 1024*64
		class socket;
		enum iocp_action {
			IDLE,
			READ,
			WRITE,
			ACCEPT,
			CONNECT
		};

		enum iocp_action_flag {
			F_READ		= 0x01,
			F_WRITE		= 0x02,
			F_ACCEPT	= 0x04,
			F_CONNECT	= 0x08
		};

		enum iocp_overlapped_ctx_type {
			IOCP_OVERLAPPED_CTX_ACCEPT = 0,
			IOCP_OVERLAPPED_CTX_READ,
			IOCP_OVERLAPPED_CTX_WRITE,
			IOCP_OVERLAPPED_CTX_CONNECT,
			IOCP_OVERLAPPED_CTX_MAX
		};

		struct iocp_overlapped_ctx {
			WSAOVERLAPPED overlapped;
			int fd;
			int parent_fd;
			iocp_action action;
			WSABUF wsabuf;
			char buf[WAWO_IOCP_BUFFER_SIZE];
			fn_io_event fn;
		};

		inline WWSP<iocp_overlapped_ctx> iocp_make_ctx() {
			WWSP<iocp_overlapped_ctx> ctx = wawo::make_shared<iocp_overlapped_ctx>();
			memset(&ctx->overlapped, 0, sizeof(WSAOVERLAPPED));
			ctx->fd = -1;
			ctx->parent_fd = -1;
			ctx->action = IDLE;
			ctx->wsabuf = { WAWO_IOCP_BUFFER_SIZE, (char*)&ctx->buf };
			return ctx;
		}

		struct iocp_ctxs {
			int fd;
			int flag;
			WWSP<iocp_overlapped_ctx> ol_ctxs[IOCP_OVERLAPPED_CTX_MAX];
		};

		inline void iocp_reset_ctx(WWSP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			memset(&ctx->overlapped, 0, sizeof(WSAOVERLAPPED));
		}

		inline void iocp_reset_accept_ctx(WWSP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			memset(&ctx->overlapped, 0, sizeof(WSAOVERLAPPED));
			ctx->fd = 0;
		}

		typedef std::map<int, WWSP<iocp_ctxs>> iocp_ctxs_map;
		typedef std::map<int, WWSP<iocp_ctxs>> iocp_ctxs_pair;

		HANDLE m_handle;
		iocp_ctxs_map m_ctxs;
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
			BOOL postrt = ::PostQueuedCompletionStatus(m_handle, -1,(DWORD)NULL,0 );
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
			BOOL getOk = ::GetQueuedCompletionStatus(m_handle, &len, (LPDWORD)&ii, (LPOVERLAPPED*)&lpol, dwWaitMill);
			if (bLastWait) {
				_after_wait();
			}
			
			if (WAWO_UNLIKELY(getOk==0)) {
				ec = wawo::socket_get_last_errno();
				if (ec == wawo::E_WAIT_TIMEOUT ) {
					WAWO_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec );
					return;
				}
				WAWO_ASSERT(len == 0);
				WAWO_ASSERT(ec != 0);

				WAWO_INFO("[iocp]GetQueuedCompletionStatus failed, %d", wawo::socket_get_last_errno());
			}
			if (len == -1) {
				WAWO_INFO("[iocp]GetQueuedCompletionStatus waken signal");
				return;
			}
			WAWO_ASSERT(lpol != NULL);

			iocp_overlapped_ctx* ctx = CONTAINING_RECORD(lpol, iocp_overlapped_ctx, overlapped);
			WAWO_ASSERT(ctx != NULL);

			if (ec == wawo::E_ERROR_NETNAME_DELETED) {
				DWORD dwTrans = 0;
				DWORD dwFlags = 0;
				if (FALSE == ::WSAGetOverlappedResult(ctx->fd, &ctx->overlapped, &dwTrans, FALSE, &dwFlags)) {
					ec = ::WSAGetLastError();
					WAWO_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", ctx->fd, dwTrans, dwFlags, ec);
				}
			}
			
			switch (ctx->action) {
				case ACCEPT:
				{
					WAWO_ASSERT(ctx->fd > 0);
					WAWO_ASSERT(ctx->parent_fd > 0);
					WAWO_ASSERT(ctx->fn != nullptr);
					int rt = ::setsockopt(ctx->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&ctx->parent_fd, sizeof(ctx->parent_fd));
					int newfd = ctx->fd;
					do_watch(IOE_ACCEPT, ctx->parent_fd, ctx->fn);
					if (WAWO_LIKELY(rt == 0)) {
						ctx->fn({ AIO_ACCEPT, newfd, NULL });
					} else {
						WAWO_CLOSE_SOCKET(newfd);
					}
				}
				break;
				case READ:
				{
					do_watch(IOE_READ, ctx->fd, ctx->fn);
					WAWO_ASSERT(ctx->fd > 0);
					WAWO_ASSERT(ctx->parent_fd == -1);
					ctx->fn({AIO_READ, ec==0?(int)len:ec, ctx->wsabuf.buf });
				}
				break;
				case WRITE:
				{
					WAWO_ASSERT(ctx->fd > 0);
					WAWO_ASSERT(ctx->parent_fd == -1);
					ctx->fn({ AIO_WRITE, ec == 0 ? (int)len : ec, NULL });
				}
				break;
				default:
				{
					WAWO_THROW("unknown io event flag");
				}
			}
			
		}

		void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn) {
			WAWO_ASSERT(fn != nullptr);

			WWSP<iocp_ctxs> _iocp_ctxs;
			iocp_ctxs_map::iterator it = m_ctxs.find(fd);
			if (it == m_ctxs.end()) {
				_iocp_ctxs = wawo::make_shared<iocp_ctxs>();
				_iocp_ctxs->fd = fd;
				_iocp_ctxs->flag = 0;
				m_ctxs.insert({ fd, _iocp_ctxs });
			} else {
				_iocp_ctxs = it->second;
			}

			if (flag&IOE_IOCP_BIND) {
				WAWO_DEBUG("[#%d][CreateIoCompletionPort] add", fd );
				WAWO_ASSERT(m_handle != NULL);
				HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)0, 0);
				WAWO_ASSERT(bindcp == m_handle);
				return;
			}

			if (flag&IOE_ACCEPT) {
				if (_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT] == NULL ) {
					_iocp_ctxs->flag |= F_ACCEPT;
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT] = iocp_make_ctx();
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT]->action = ACCEPT;
				} else {
					iocp_reset_accept_ctx( _iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT] );
				}
				_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT]->fn = fn;

				//listen
				LPFN_ACCEPTEX lpfnAcceptEx = NULL;
				GUID GuidAcceptEx = WSAID_ACCEPTEX;
				DWORD dwbytes;

				int iResult = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
					&GuidAcceptEx, sizeof(GuidAcceptEx),
					&lpfnAcceptEx, sizeof(lpfnAcceptEx),
					&dwbytes, NULL, NULL);

				if (iResult == SOCKET_ERROR) {
					fn({ AIO_ACCEPT, wawo::socket_get_last_errno(), NULL });
					return;
				}

				WSAPROTOCOL_INFO proto_info;
				int info_size = sizeof(proto_info);
				int ret = ::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL_INFOA, (char *)&proto_info, &info_size);
				if (ret == SOCKET_ERROR) {
					fn({ AIO_ACCEPT, wawo::socket_get_last_errno(), NULL });
					return;
				}

				int so_family = proto_info.iAddressFamily;
				int so_type = proto_info.iSocketType;
				int so_proto = proto_info.iProtocol;

				int newfd = ::WSASocketW(so_family, so_type, so_proto,NULL, 0, WSA_FLAG_OVERLAPPED);
				if (newfd == -1) {
					fn({ AIO_ACCEPT, wawo::socket_get_last_errno(), NULL });
					return;
				}

				WWSP<iocp_overlapped_ctx>& ctx = _iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT];
				ctx->fd = newfd;
				ctx->parent_fd = fd;

				BOOL acceptrt = lpfnAcceptEx(fd, newfd, ctx->buf, 0,
					sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
					NULL, &(ctx->overlapped));

				if (!acceptrt)
				{
					int ec = wawo::socket_get_last_errno();
					if ( ec != wawo::E_ERROR_IO_PENDING) {
						WAWO_CLOSE_SOCKET(newfd);
						fn({AIO_ACCEPT, ec, NULL});
					}
				}
				return;
			}

			if (flag&IOE_READ) {
				if (_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ] == NULL) {
					_iocp_ctxs->flag |= F_READ;
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ] = iocp_make_ctx();
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ]->action = READ;
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ]->fd = fd;
				} else {
					iocp_reset_ctx(_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ]);
				}
				_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ]->fn = fn;

				//read begin
				WWSP<iocp_overlapped_ctx>& _ctx = _iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_READ];
				static DWORD flags = 0;
				if (::WSARecv(fd, &_ctx->wsabuf, 1, NULL, &flags, &_ctx->overlapped, NULL) == SOCKET_ERROR) {
					int ec = wawo::socket_get_last_errno();
					if (ec != wawo::E_ERROR_IO_PENDING) {
						fn({AIO_READ, ec, NULL});
					}
				}
			}
		}

		void do_unwatch(u8_t const& flag, int const& fd)
		{
		
		}

		void do_WSASend(int const& fd, fn_io_event_wsa_send const& fn_wsasend, fn_io_event const& fn) {
			WWSP<iocp_ctxs> _iocp_ctxs;
			iocp_ctxs_map::iterator it = m_ctxs.find(fd);
			if (it == m_ctxs.end()) {
				_iocp_ctxs = wawo::make_shared<iocp_ctxs>();
				_iocp_ctxs->fd = fd;
				_iocp_ctxs->flag = 0;
				m_ctxs.insert({ fd, _iocp_ctxs });
			}
			else {
				_iocp_ctxs = it->second;
			}
			WAWO_ASSERT(_iocp_ctxs != NULL);

			if (_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE] == NULL) {
				_iocp_ctxs->flag |= F_WRITE;
				_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE] = iocp_make_ctx();
				_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE]->action = WRITE;
				_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE]->fd = fd;
			}
			else {
				iocp_reset_ctx(_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE]);
			}
			_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE]->fn = fn;

			int wsasndrt = fn_wsasend((void*)&_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_WRITE]->overlapped);
			if (wsasndrt != wawo::OK) {
				fn({AIO_WRITE, wsasndrt});
			}
		}
	};
}}}
#endif