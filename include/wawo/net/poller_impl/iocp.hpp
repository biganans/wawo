#ifndef _WAWO_NET_POLLER_IMPL_IOCP_HPP
#define _WAWO_NET_POLLER_IMPL_IOCP_HPP

#include <wawo/core.hpp>
#include <wawo/net/poller_abstract.hpp>

#include <mswsock.h>

namespace wawo { namespace net { namespace impl {

	class iocp :
		public poller_abstract
	{

#define WAWO_IOCP_BUFFER_SIZE 1024*64
		class socket;
		enum iocp_action {
			READ = 0,
			WRITE,
			ACCEPT,
			CONNECT,
			MAX
		};

		enum action_status {
			IDLE,
			IN_OPERATION,
			BLOCKED,
		};

		enum iocp_action_flag {
			F_READ		= 0x01,
			F_WRITE		= 0x02,
			F_ACCEPT	= 0x04,
			F_CONNECT	= 0x08
		};

		struct iocp_overlapped_ctx:
			public ref_base
		{
			WSAOVERLAPPED overlapped;
			int fd;
			int parent_fd;
			int action:24;
			int action_status:8;
			fn_io_event fn;
			WSABUF wsabuf;
			char buf[WAWO_IOCP_BUFFER_SIZE];
		};

		inline WWRP<iocp_overlapped_ctx> iocp_make_ctx() {
			WWRP<iocp_overlapped_ctx> ctx = wawo::make_ref<iocp_overlapped_ctx>();
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
			ctx->fd = -1;
			ctx->parent_fd = -1;
			ctx->action = -1;
			ctx->wsabuf = { WAWO_IOCP_BUFFER_SIZE, (char*)&ctx->buf };
			return ctx;
		}

		struct iocp_ctxs {
			int fd;
			int flag;
			WWRP<iocp_overlapped_ctx> ol_ctxs[iocp_action::MAX];
		};

		inline void iocp_reset_ctx(WWRP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
		}

		inline void iocp_reset_accept_ctx(WWRP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			::memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
			ctx->fd = -1;
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
			m_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (u_long) 0, 1);
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
				if (ec == wawo::E_WAIT_TIMEOUT ||
					ec == wawo::E_ERROR_CONNECTION_ABORTED
					) {
					WAWO_DEBUG("[iocp]GetQueuedCompletionStatus return: %d", ec );
					return;
				}
				//did not dequeue a completion packet from the completion port
				if (lpol == NULL) { 
					WAWO_DEBUG("[iocp]GetQueuedCompletionStatus return: %d, no packet dequeue", ec );
					return;
				}
				WWRP<iocp_overlapped_ctx> _ctx(CONTAINING_RECORD(lpol, iocp_overlapped_ctx, overlapped));

				WAWO_ASSERT(len == 0);
				WAWO_ASSERT(ec != 0);
				WAWO_INFO("[iocp]GetQueuedCompletionStatus failed, %d", wawo::socket_get_last_errno());

			} else {
				if (len == -1) {
					WAWO_INFO("[iocp]GetQueuedCompletionStatus waken signal");
					return;
				}
				WAWO_ASSERT(lpol != NULL);
			}
			WWRP<iocp_overlapped_ctx> ctx(CONTAINING_RECORD(lpol, iocp_overlapped_ctx, overlapped));
			WAWO_ASSERT(ctx != NULL);

			if ( WAWO_UNLIKELY(ec == wawo::E_ERROR_NETNAME_DELETED)) {
				DWORD dwTrans = 0;
				DWORD dwFlags = 0;
				if (FALSE == ::WSAGetOverlappedResult(ctx->fd, &ctx->overlapped, &dwTrans, FALSE, &dwFlags)) {
					ec = wawo::socket_get_last_errno();
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
					if (WAWO_LIKELY(rt == 0)) {
						ctx->fn({ AIO_ACCEPT, ctx->fd, NULL });
					} else {
						WAWO_CLOSE_SOCKET(ctx->fd);
					}
					if (ctx->action_status != BLOCKED) {
						do_watch(IOE_ACCEPT, ctx->parent_fd, ctx->fn);
					}
				}
				break;
				case READ:
				{
					WAWO_ASSERT(ctx->fd > 0);
					WAWO_ASSERT(ctx->parent_fd == -1);
					ctx->fn({AIO_READ, ec==0?(int)len:ec, ctx->wsabuf.buf });
					if (ctx->action_status != BLOCKED) {
						do_watch(IOE_READ, ctx->fd, ctx->fn);
					}
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

			if (flag&IOE_IOCP_INIT) {
				WAWO_DEBUG("[#%d][CreateIoCompletionPort]init", fd );
				WAWO_ASSERT(m_handle != NULL);
				HANDLE bindcp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)0, 0);
				WAWO_ASSERT(bindcp == m_handle);

				iocp_ctxs_map::iterator it = m_ctxs.find(fd);
				WAWO_ASSERT(it == m_ctxs.end());
				WWSP<iocp_ctxs> _iocp_ctxs = wawo::make_shared<iocp_ctxs>();
				_iocp_ctxs->fd = fd;
				_iocp_ctxs->flag = 0;

				_iocp_ctxs->ol_ctxs[READ] = iocp_make_ctx();
				_iocp_ctxs->ol_ctxs[READ]->action = READ;
				_iocp_ctxs->ol_ctxs[READ]->fd = fd;
				_iocp_ctxs->ol_ctxs[READ]->action_status = IDLE;

				_iocp_ctxs->ol_ctxs[WRITE] = iocp_make_ctx();
				_iocp_ctxs->ol_ctxs[WRITE]->action = WRITE;
				_iocp_ctxs->ol_ctxs[WRITE]->fd = fd;
				_iocp_ctxs->ol_ctxs[WRITE]->action_status = IDLE;

				_iocp_ctxs->ol_ctxs[ACCEPT] = iocp_make_ctx();
				_iocp_ctxs->ol_ctxs[ACCEPT]->action = ACCEPT;
				_iocp_ctxs->ol_ctxs[ACCEPT]->fd = fd;
				_iocp_ctxs->ol_ctxs[ACCEPT]->action_status = IDLE;

				_iocp_ctxs->ol_ctxs[CONNECT] = iocp_make_ctx();
				_iocp_ctxs->ol_ctxs[CONNECT]->action = CONNECT;
				_iocp_ctxs->ol_ctxs[CONNECT]->fd = fd;
				_iocp_ctxs->ol_ctxs[CONNECT]->action_status = IDLE;

				m_ctxs.insert({ fd, _iocp_ctxs });
				return;
			}

			if (flag&IOE_IOCP_DEINIT) {
				iocp_ctxs_map::iterator it = m_ctxs.find(fd);
				WAWO_ASSERT(it != m_ctxs.end());
				m_ctxs.erase(it);
				WAWO_DEBUG("[#%d][CreateIoCompletionPort]deinit", fd);
				return;
			}

			WAWO_ASSERT(fn != nullptr);
			iocp_ctxs_map::iterator it = m_ctxs.find(fd);
			if (it == m_ctxs.end()) {
				return;
			}
			WWSP<iocp_ctxs> _iocp_ctxs = it->second;

			if (flag&IOE_ACCEPT) {
				WAWO_ASSERT(_iocp_ctxs->ol_ctxs[ACCEPT] != NULL);
				WWRP<iocp_overlapped_ctx>& ctx = _iocp_ctxs->ol_ctxs[ACCEPT];
				_iocp_ctxs->flag |= F_ACCEPT;
				iocp_reset_accept_ctx(ctx);
				ctx->fn = fn;

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
				//WAWO_INFO("[iocp]creating new fd: %d", newfd);
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
						ctx->fd = -1;
						fn({AIO_ACCEPT, ec, NULL});
						return;
					}
				}
				ctx->action_status = IN_OPERATION;
				return;
			}

			if (flag&IOE_READ) {
				WWRP<iocp_overlapped_ctx>& ctx = _iocp_ctxs->ol_ctxs[READ];
				WAWO_ASSERT(ctx != NULL);

				_iocp_ctxs->flag |= F_READ;
				iocp_reset_ctx(ctx);
				ctx->fn = fn;

				//read begin
				static DWORD flags = 0;
				if (::WSARecv(fd, &ctx->wsabuf, 1, NULL, &flags, &ctx->overlapped, NULL) == SOCKET_ERROR) {
					int ec = wawo::socket_get_last_errno();
					if (ec != wawo::E_ERROR_IO_PENDING) {
						fn({AIO_READ, ec, NULL});
						return;
					}
				}
				ctx->action_status = IN_OPERATION;
			}
		}

		void do_unwatch(u8_t const& flag, int const& fd)
		{
		
		}

		void do_IOCP_overlapped_call(u8_t const& flag, int const& fd, fn_overlapped_io_event const& fn_overlapped, fn_io_event const& fn) {
			iocp_ctxs_map::iterator it = m_ctxs.find(fd);
			WAWO_ASSERT(it != m_ctxs.end());
			WWSP<iocp_ctxs> _iocp_ctxs = it->second;
			
			WAWO_ASSERT(_iocp_ctxs != NULL);
			
			if (flag&IOE_WRITE) {
				WWRP<iocp_overlapped_ctx>& ctx = _iocp_ctxs->ol_ctxs[WRITE];

				WAWO_ASSERT(ctx != NULL);
				_iocp_ctxs->flag |= F_WRITE;
				iocp_reset_ctx(ctx);
				ctx->fn = fn;

				int wsasndrt = fn_overlapped((void*)&ctx->overlapped);
				if (wsasndrt != wawo::OK) {
					fn({ AIO_WRITE, wsasndrt, NULL });
				}
				return;
			}

			if (flag&IOE_CONNECT) {
				WWRP<iocp_overlapped_ctx>& ctx = _iocp_ctxs->ol_ctxs[CONNECT];

				WAWO_ASSERT(ctx != NULL);
				_iocp_ctxs->flag |= F_CONNECT;
				iocp_reset_ctx(ctx);
				ctx->fn = fn;

				int wsasndrt = fn_overlapped((void*)&ctx->overlapped);
				if (wsasndrt != wawo::OK) {
					fn({ AIO_CONNECT, wsasndrt, NULL });
				}
				return;
			}
		}
	};
}}}
#endif