#ifndef WAWO_NET_SOCKET_API_HPP
#define WAWO_NET_SOCKET_API_HPP

#include <wawo/core.hpp>

#ifdef WAWO_ENABLE_TRACE_SOCKET_API
#define WAWO_ENABLE_TRACE_SOCKET_API WAWO_INFO
#else
#define WAWO_ENABLE_TRACE_SOCKET_API(...)
#endif

//#ifndef EWOULDBLOCK
//#define EWOULDBLOCK WSAEWOULDBLOCK
//#endif

//#ifndef WSAEWOULDBLOCK
//#define WSAEWOULDBLOCK EWOULDBLOCK
//#endif

//#ifndef EAGAIN //EAGAIN IS NOT ALWAYS THE SAME AS EWOULDBLOCK
//#define EAGAIN EWOULDBLOCK
//#endif

//#ifndef EISCONN
//#define EISCONN WSAEISCONN
//#endif

//#ifndef ENOTINITIALISED
//#define ENOTINITIALISED WSANOTINITIALISED
//#endif

#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==wawo::E_EAGAIN)||(_errno==wawo::E_EWOULDBLOCK)||(_errno==wawo::E_WSAEWOULDBLOCK))
#define IS_ERRNO_EQUAL_CONNECTING(_errno) ((_errno==wawo::E_EINPROGRESS)||(_errno==wawo::E_WSAEWOULDBLOCK))

namespace wawo { namespace net { namespace socket_api {
	typedef int(*fn_socket)(int const& family, int const& socket_type, int const& protocol);
	typedef int(*fn_connect)(int const& fd, const struct sockaddr* addr, socklen_t const& length);
	typedef int(*fn_bind)(int const& fd, const struct sockaddr* addr, socklen_t const& length);
	typedef int(*fn_shutdown)(int const& fd, int const& flag);
	typedef int(*fn_close)(int const& fd);
	typedef int(*fn_listen)(int const& fd, int const& backlog);
	typedef int(*fn_accept)(int const& fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef int(*fn_getsockopt)(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len);
	typedef int(*fn_setsockopt)(int const& fd, int const& level, int const& option_name, void const* optval, socklen_t const& option_len);
	typedef int(*fn_getsockname)(int const& fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef u32_t(*fn_send)(int const& fd, byte_t const* const buffer, u32_t const& length, int& ec_o, int const& flag);
	typedef u32_t(*fn_recv)(int const&fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag);
	typedef u32_t(*fn_sendto)(int const& fd, wawo::byte_t const* const buff, wawo::u32_t const& length, const wawo::net::address& addr, int& ec_o, int const& flag);
	typedef u32_t(*fn_recvfrom)(int const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag);

	namespace standard {
		inline int socket(int const& family, int const& socket_type, int const& protocol) {
			int rt = ::socket(family, socket_type, protocol);
			WAWO_RETURN_V_IF_MATCH(rt, rt > 0);
			return socket_get_last_errno();
		}

		inline int connect(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
			int rt = ::connect(fd, addr, length);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int bind(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
			int rt = ::bind(fd, addr, length);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int shutdown(int const& fd, int const& flag) {
			int rt = ::shutdown(fd, flag);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int close(int const& fd) {
			int rt = WAWO_CLOSE_SOCKET(fd);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int listen(int const& fd, int const& backlog) {
			int rt = ::listen(fd, backlog);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int accept(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			int accepted_fd = ::accept(fd, addr, addrlen);
			WAWO_RETURN_V_IF_MATCH(accepted_fd, (accepted_fd > 0));
			return socket_get_last_errno();
		}

		inline int getsockopt(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len) {
			int rt = ::getsockopt(fd, level, option_name, (char*)value, option_len);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int setsockopt(int const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_len) {
			int rt = ::setsockopt(fd, level, option_name, (char*)value, option_len);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline int getsockname(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			int rt = ::getsockname(fd, addr, addrlen);
			WAWO_RETURN_V_IF_MATCH(0, rt == 0);
			return socket_get_last_errno();
		}

		inline u32_t send(int const& fd, byte_t const* const buffer, u32_t const& len, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);

			u32_t R = 0;

			//TRY SEND
			do {
				int r = ::send(fd, reinterpret_cast<const char*>(buffer) + R, len - R, flag);
				if (WAWO_LIKELY(r>0)) {
					ec_o = wawo::OK;
					R += r;
					if (R == len) {
						break;
					}
				}
				else {

					WAWO_ASSERT(r == -1);
					int ec = socket_get_last_errno();
					if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
						ec_o = wawo::E_CHANNEL_WRITE_BLOCK;
						//WAWO_TRACE_SOCKET("[wawo::net::send][#%d]send blocked, error code: <%d>", fd, ec);
					}
					else if ((ec) == wawo::E_EINTR) {
						continue;
					}
					else {
						WAWO_ERR("[wawo::net::send][#%d]send failed, error code: <%d>", fd, ec);
						ec_o = (ec);
					}
					break;
				}
			} while (true);

			WAWO_ENABLE_TRACE_SOCKET_API("[wawo::net::send][#%d]send, to send: %d, sent: %d, ec: %d", fd, len, sent_total, ec_o);
			return R;
		}

		inline u32_t recv(int const&fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);

			u32_t r_total = 0;
			do {
				int r = ::recv(fd, reinterpret_cast<char*>(buffer_o) + r_total, size - r_total, flag);
				if (WAWO_LIKELY(r>0)) {
					r_total += r;
					ec_o = wawo::OK;
					break;
				}
				else if (r == 0) {
					WAWO_ENABLE_TRACE_SOCKET_API("[wawo::net::recv][#%d]socket closed by remote side gracefully[detected by recv]", fd);
					ec_o = wawo::E_SOCKET_GRACE_CLOSE;
					break;
				}
				else {
					WAWO_ASSERT(r == -1);
					int ec = socket_get_last_errno();
					if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
						//WAWO_TRACE_SOCKET("[wawo::net::recv][#%d]recv blocked, block code: <%d>", fd, ec);
						ec_o = wawo::E_CHANNEL_READ_BLOCK;
					}
					else if (ec == wawo::E_EINTR) {
						continue;
					}
					else {
						WAWO_ASSERT(ec != wawo::OK);
						ec_o = ec;
						WAWO_ERR("[wawo::net::recv][#%d]recv error, errno: %d", fd, ec);
					}
					break;
				}
			} while (true);

			WAWO_ENABLE_TRACE_SOCKET_API("[wawo::net::recv][#%d]recv bytes, %d", fd, r_total);
			return r_total;
		}

		inline u32_t sendto(int const& fd, wawo::byte_t const* const buff, wawo::u32_t const& len, const wawo::net::address& addr, int& ec_o, int const& flag) {

			WAWO_ASSERT(buff != NULL);
			WAWO_ASSERT(len > 0);

			sockaddr_in addr_in;
			ec_o = wawo::OK;

			WAWO_ASSERT(addr.nip() != 0);

			addr_in.sin_family = AF_INET;
			addr_in.sin_addr.s_addr = addr.nip();
			addr_in.sin_port = addr.nport();

			u32_t sent_total = 0;

			do {
				int sent = ::sendto(fd, reinterpret_cast<const char*>(buff), len, flag, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));

				if (WAWO_LIKELY(sent>0)) {
					WAWO_ASSERT((u32_t)sent == len);
					ec_o = wawo::OK;
					sent_total = sent;
					break;
				}

				WAWO_ASSERT(sent == -1);
				int send_ec = socket_get_last_errno();
				if (IS_ERRNO_EQUAL_WOULDBLOCK(send_ec)) {
					ec_o = wawo::E_CHANNEL_WRITE_BLOCK;
					//WAWO_TRACE_SOCKET("[wawo::net::sendto][#%d]send blocked, error code: <%d>, no retry", fd, send_ec);
				}
				else if (send_ec == wawo::E_EINTR) {
					continue;
				}
				else {
					WAWO_ERR("[wawo::net::sendto][#%d]send failed, error code: <%d>", fd, send_ec);
					ec_o = WAWO_NEGATIVE(send_ec);
				}
				break;

			} while (true);

			WAWO_ENABLE_TRACE_SOCKET_API("[wawo::net::sendto][#%d]sendto() == %d", fd, sent_total);
			return sent_total;
		}

		inline u32_t recvfrom(int const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag) {

			sockaddr_in addr_in;
			u32_t r_total;
			do {
				socklen_t socklen = sizeof(addr_in);
				int nbytes = ::recvfrom(fd, reinterpret_cast<char*>(buff_o), size, flag, reinterpret_cast<sockaddr*>(&addr_in), &socklen);

				if (WAWO_LIKELY(nbytes>0)) {
					r_total = nbytes;
					addr_o.setnport((addr_in.sin_port));
					addr_o.setnip(addr_in.sin_addr.s_addr);
					ec_o = wawo::OK;
					break;
				}

				WAWO_ASSERT(nbytes == -1);
				int _ern = socket_get_last_errno();
				if (IS_ERRNO_EQUAL_WOULDBLOCK(_ern)) {
					ec_o = E_CHANNEL_READ_BLOCK;
				}
				else if (_ern == wawo::E_EINTR) {
					continue;
				}
				else {
					ec_o = WAWO_NEGATIVE(_ern);
					WAWO_ERR("[wawo::net::recvfrom][#%d]recvfrom, ERROR: %d", fd, _ern);
				}

				r_total = 0;
				break;
			} while (true);

			WAWO_ENABLE_TRACE_SOCKET_API("[wawo::net::recvfrom][#%d]recvfrom() == %d", fd, r_total);
			return r_total;
		}
	}

#ifdef WAWO_ENABLE_IOCP
	namespace iocp {
		inline int socket(int const& family, int const& socket_type, int const& protocol) {
			int rt = ::WSASocketW(family, socket_type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
			WAWO_RETURN_V_IF_MATCH(rt, rt > 0);
			return socket_get_last_errno();
		}
	}
#endif

#ifdef WAWO_ENABLE_WCP
#include <wawo/net/wcp.hpp>
	namespace wcp {
		inline int socket(int const& family, int const& socket_type, int const& protocol) {
			return wcp::instance()->socket(family, socket_type, protocol);
		}

		inline int connect(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
			return wcp::instance()->connect(fd, addr, length);
		}

		inline int bind(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
			return wcp::instance()->bind(fd, addr, length);
		}

		inline int shutdown(int const& fd, int const& flag) {
			return wcp::instance()->shutdown(fd, flag);
		}

		inline int close(int const& fd) {
			return wcp::instance()->close(fd);
		}
		inline int listen(int const& fd, int const& backlog) {
			return wcp::instance()->listen(fd, backlog);
		}

		inline int accept(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			return wcp::instance()->accept(fd, addr, addrlen);
		}

		inline int getsockopt(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len) {
			return wcp::instance()->getsockopt(fd, level, option_name, value, option_len);
		}

		inline int setsockopt(int const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_len) {
			return wcp::instance()->setsockopt(fd, level, option_name, value, option_len);
		}

		inline int getsockname(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			return wcp::instance()->getsockname(fd, addr, addrlen);
		}

		inline u32_t send(int const& fd, byte_t const* const buffer, u32_t const& len, int& ec_o, int const& flag = 0) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);

			u32_t sent_total = 0;

			do {
				int r = wcp::instance()->send(fd, (buffer + sent_total), (len - sent_total), flag);
				if (WAWO_LIKELY(r>0)) {
					ec_o = wawo::OK;
					sent_total += r;
					if (sent_total == len) {
						break;
					}
				}
				else {
					WAWO_ASSERT(r < 0);
					if (IS_ERRNO_EQUAL_WOULDBLOCK(WAWO_ABS(r))) {
						ec_o = wawo::E_CHANNEL_WRITE_BLOCK;
					}
					else {
						ec_o = r;
					}
					break;
				}
			} while (true);

			WAWO_TRACE_SOCKET_INOUT("[wawo::net::wcp_socket::send][#%d]send() == %d", fd, r_total);
			return sent_total;
		}

		inline u32_t recv(int const& fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0) {

			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);
			u32_t r_total = 0;

			do {
				int r = wcp::instance()->recv(fd, buffer_o + r_total, size - r_total, flag);

				if (WAWO_LIKELY(r>0)) {
					r_total += r;
					ec_o = wawo::OK;
					break;
				}
				else if (r == 0) {
					ec_o = wawo::E_SOCKET_GRACE_CLOSE;
					break;
				}
				else {
					if (IS_ERRNO_EQUAL_WOULDBLOCK(WAWO_ABS(r))) {
						ec_o = wawo::E_CHANNEL_READ_BLOCK;
					}
					else if (WAWO_ABS(r) == EINTR) {
						continue;
					}
					else {
						ec_o = WAWO_NEGATIVE(r);
						WAWO_ERR("[wawo::net::wcp_socket::recv][#%d]recv error, errno: %d", fd, r);
					}
					break;
				}
			} while (true);

			WAWO_TRACE_SOCKET_INOUT("[wawo::net::wcp_socket::recv][#%d]recv() == %d", fd, r_total);
			return r_total;
		}

		enum WCP_FcntlCommand {
			WCP_F_GETFL = 0x01,
			WCP_F_SETFL = 0x02
		};

		inline int fcntl(int const& fd, int const cmd, ...) {

			switch (cmd) {

			case WCP_F_SETFL:
			{
				va_list vl;
				va_start(vl, cmd);
				int flag = va_arg(vl, int);
				va_end(vl);
				return wcp::instance()->fcntl_setfl(fd, flag);
			}
			break;

			case WCP_F_GETFL:
			{
				int flag = -1;
				wcp::instance()->fcntl_getfl(fd, flag);
				return flag;
			}
			break;
			default:
			{
				wawo::set_last_errno(wawo::E_WCP_WPOLL_INVALID_OP);
				return -1;
			}
			}
		}
	}
#endif

}}}

#endif