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
			WSAOVERLAPPED _overlapped;
			int fd;
			int parent_fd;
			iocp_action action;
			WSABUF wsabuf;
			char buf[WAWO_IOCP_BUFFER_SIZE];
			int len;
			int ec;
			fn_io_event fn;
		};

		inline WWSP<iocp_overlapped_ctx> iocp_make_ctx() {
			WWSP<iocp_overlapped_ctx> _ctx = wawo::make_shared<iocp_overlapped_ctx>();
			memset(&_ctx->_overlapped, 0, sizeof(WSAOVERLAPPED));
			_ctx->fd = -1;
			_ctx->parent_fd = -1;
			_ctx->action = IDLE;
			_ctx->len = 0;
			_ctx->ec = 0;
			_ctx->wsabuf = { WAWO_IOCP_BUFFER_SIZE, (char*)&_ctx->buf };
		}

		struct iocp_ctxs {
			int fd;
			int flag;
			WWSP<iocp_overlapped_ctx> ol_ctxs[IOCP_OVERLAPPED_CTX_MAX];
		};

		inline void iocp_reset_ctx(WWSP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			memset(&ctx->_overlapped, 0, sizeof(WSAOVERLAPPED));
			ctx->len = 0;
			ctx->ec = 0;
		}

		inline void iocp_reset_accept_ctx(WWSP<iocp_overlapped_ctx> const& ctx) {
			WAWO_ASSERT(ctx != NULL);
			memset(&ctx->_overlapped, 0, sizeof(WSAOVERLAPPED));
			ctx->fd = 0;
		}

		/*
		inline void iocp_accept_done() {
			WAWO_ASSERT(m_state == S_OPENED);
			m_sm = SM_PASSIVE;
			m_state = S_CONNECTED; //compatible for __cb_async_connected
			int rt = turnon_nonblocking();
			WAWO_ASSERT(rt == wawo::OK);

			channel::ch_fire_connected();
			iocp_bind();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void iocp_read_done() {
			WWSP<iocp_overlapped_ctx>& ctx = m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_READ];
			WAWO_ASSERT(ctx != NULL);

			if (ctx->ec != 0) {
				if (ctx->ec == ERROR_NETNAME_DELETED) {
					DWORD dwTrans = 0;
					DWORD dwFlags = 0;
					if (FALSE == ::WSAGetOverlappedResult(fd(), &ctx->_overlapped, &dwTrans, FALSE, &dwFlags)) {
						ctx->ec = ::WSAGetLastError();
						WAWO_DEBUG("[#%d]dwTrans: %d, dwFlags: %d, update ec: %d", fd(), dwTrans, dwFlags, ctx->ec);
					}
				}
				WAWO_ASSERT(ctx->len == 0);
				ch_errno(ctx->ec);
				ch_close();
				return;
			}

			if (ctx->len == 0) {
				ch_shutdown_read();
				return;
			}

			WAWO_ASSERT(ctx->len>0);
			WWRP<wawo::packet> income = wawo::make_ref<wawo::packet>(ctx->len);
			WAWO_ASSERT(m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_READ] != NULL);
			income->write((byte_t*)m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_READ]->buf, ctx->len);
			channel::ch_read(income);
			end_read();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void iocp_write_done() {
			WWSP<iocp_overlapped_ctx>& ctx = m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_WRITE];

			WAWO_ASSERT(m_outbound_entry_q.size());
			WAWO_ASSERT(ctx->len > 0);
			WAWO_ASSERT(m_noutbound_bytes > 0);
			socket_outbound_entry entry = m_outbound_entry_q.front();
			WAWO_ASSERT(entry.data != NULL);
			entry.data->skip(ctx->len);
			m_noutbound_bytes -= ctx->len;
			if (entry.data->len() == 0) {
				entry.ch_promise->set_success(wawo::OK);
				m_outbound_entry_q.pop();
			}
			ch_flush_impl();
		}
		*/

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
			ctx->len = len;
			ctx->ec = ec;
			
			switch (ctx->action) {
				case ACCEPT:
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
				case READ:
				{
					WAWO_ASSERT(ctx->ref_so == NULL);
					WAWO_ASSERT(ctx->so != NULL);
					so->iocp_read_done();
				}
				break;
				case WRITE:
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
			*/
		}

		void do_watch(u8_t const& flag, int const& fd, fn_io_event const& fn ) {

			WAWO_ASSERT(fn != nullptr);

			if (flag&IOE_IOCP_BIND) {
				WAWO_DEBUG("[#%d][CreateIoCompletionPort] add", so->fd() );
				HANDLE new_so_cp = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)0, 0);
				WAWO_ASSERT(new_so_cp == m_handle);
				return;
			}

			if (flag&IOE_ACCEPT) {
				WWSP<iocp_ctxs> _iocp_ctxs;
				iocp_ctxs_map::iterator it = m_ctxs.find(fd);
				if (it == m_ctxs.end()) {
					WAWO_ASSERT(m_handle != NULL);
					HANDLE listencpport = ::CreateIoCompletionPort((HANDLE)fd, m_handle, (u_long)0, 0);
					WAWO_ASSERT(listencpport == m_handle);

					_iocp_ctxs = wawo::make_shared<iocp_ctxs>();
					_iocp_ctxs->fd = fd;
					_iocp_ctxs->flag = F_ACCEPT;
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT] = iocp_make_ctx();
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT]->action = ACCEPT;
					_iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT]->fn = fn;

					m_ctxs.insert({fd, _iocp_ctxs});
				} else {
					_iocp_ctxs = it->second;
					iocp_reset_accept_ctx( _iocp_ctxs->ol_ctxs[IOCP_OVERLAPPED_CTX_ACCEPT] );
				}

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
				ctx->action = ACCEPT;
				ctx->fd = newfd;
				ctx->parent_fd = fd;

				BOOL acceptrt = lpfnAcceptEx(fd, newfd, ctx->buf, 0,
					sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
					NULL, &(ctx->_overlapped));

				if (!acceptrt)
				{
					int ec = wawo::socket_get_last_errno();
					if (ec != ERROR_IO_PENDING) {
						WAWO_CLOSE_SOCKET(newfd);
						fn({AIO_ACCEPT, ec, NULL});
						return;
					}
				}
			}

			if (flag&IOE_READ) {

				/*
				//read begin
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
				_ctx->action = READ;
				*/
			}
		}

		void do_unwatch(u8_t const& flag, int const& fd)
		{
		
		}
	};
}}}
#endif