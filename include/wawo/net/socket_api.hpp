 #ifndef WAWO_NET_SOCKET_API_HPP
#define WAWO_NET_SOCKET_API_HPP

#include <wawo/core.hpp>
#include <wawo/net/address.hpp>

#ifdef WAWO_PLATFORM_WIN
	#include <wawo/net/winsock_helper.hpp>
#endif

#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==wawo::E_EAGAIN)||(_errno==wawo::E_EWOULDBLOCK)||(_errno==wawo::E_WSAEWOULDBLOCK))
#define IS_ERRNO_EQUAL_CONNECTING(_errno) ((_errno==wawo::E_EINPROGRESS)||(_errno==wawo::E_WSAEWOULDBLOCK))

namespace wawo { namespace net { namespace socket_api {
	typedef SOCKET(*fn_socket)(int const& family, int const& type, int const& proto);
	typedef int(*fn_connect)(SOCKET const& fd, address const& addr );
	typedef int(*fn_bind)(SOCKET const& fd, address const& addr);
	typedef int(*fn_shutdown)(SOCKET const& fd, int const& flag);
	typedef int(*fn_close)(SOCKET const& fd);
	typedef int(*fn_listen)(SOCKET const& fd, int const& backlog);
	typedef SOCKET(*fn_accept)(SOCKET const& fd, address& addr );
	typedef int(*fn_getsockname)(SOCKET const& fd, address& addr);
	typedef int(*fn_getsockopt)(SOCKET const& fd, int const& level, int const& name, void* value, socklen_t* option_len);
	typedef int(*fn_setsockopt)(SOCKET const& fd, int const& level, int const& name, void const* optval, socklen_t const& option_len);

	typedef wawo::u32_t(*fn_send)(SOCKET const& fd, byte_t const* const buffer, wawo::u32_t const& len, int& ec_o, int const& flag);
	typedef wawo::u32_t(*fn_recv)(SOCKET const&fd, byte_t* const buffer_o, wawo::u32_t const& size, int& ec_o, int const& flag);
	typedef wawo::u32_t(*fn_sendto)(SOCKET const& fd, wawo::byte_t const* const buff, wawo::u32_t const& len, address const& addr, int& ec_o, int const& flag);
	typedef wawo::u32_t(*fn_recvfrom)(SOCKET const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag);

	namespace posix {
		inline SOCKET socket(int const& family, int const& type, int const& protocol) {
			return ::socket( OS_DEF_family[family], OS_DEF_sock_type[type], OS_DEF_protocol[protocol]);
		}

		inline int connect(SOCKET const& fd, address const& addr) {
			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in) );
			addr_in.sin_family = OS_DEF_family[addr.family()];
			addr_in.sin_port = addr.nport();
			addr_in.sin_addr.S_un.S_addr = addr.nipv4();
			return ::connect(fd, (sockaddr*)(&addr_in), sizeof(addr_in) );
		}

		inline int bind(SOCKET const& fd, address const& addr) {
			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));
			addr_in.sin_family = OS_DEF_family[addr.family()];
			addr_in.sin_port = addr.nport();
			addr_in.sin_addr.S_un.S_addr = addr.nipv4();
			return ::bind(fd, (sockaddr*)(&addr_in), sizeof(addr_in));
		}

		inline int shutdown(SOCKET const& fd, int const& flag) {
			return  ::shutdown(fd, flag);
		}

		inline int close(SOCKET const& fd) {
			return WAWO_CLOSE_SOCKET(fd);
		}

		inline int listen(SOCKET const& fd, int const& backlog) {
			return ::listen(fd, backlog);
		}

		inline SOCKET accept(SOCKET const& fd, address& addr) {
			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));
			socklen_t len = sizeof(addr_in);

			SOCKET accepted_fd = ::accept(fd, (sockaddr*)(&addr_in), &len);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, (accepted_fd == wawo::E_INVALID_SOCKET));
			addr = address(addr_in);
			return accepted_fd;
		}

		inline int getsockname(SOCKET const& fd, address& addr) {
			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));

			socklen_t len = sizeof(addr_in);
			int rt = ::getsockname(fd, (sockaddr*)(&addr_in), &len);
			WAWO_RETURN_V_IF_MATCH(rt, rt == wawo::E_SOCKET_ERROR);
			addr = address(addr_in);
			return wawo::OK;
		}

		inline int getsockopt(SOCKET const& fd, int const& level, int const& name, void* value, socklen_t* option_len) {
			return ::getsockopt(fd, level, name, (char*)value, option_len);
		}

		inline int setsockopt(SOCKET const& fd, int const& level, int const& name, void const* value, socklen_t const& option_len) {
			return ::setsockopt(fd, level, name, (char*)value, option_len);
		}

		inline wawo::u32_t send(SOCKET const& fd, byte_t const* const buffer, wawo::u32_t const& len, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);

			wawo::u32_t R = 0;

			//TRY SEND
			do {
				const int r = ::send(fd, reinterpret_cast<const char*>(buffer) + R, (int)(len - R), flag);
				if (WAWO_LIKELY(r > 0)) {
					ec_o = wawo::OK;
					R += r;
					if (R == len) {
						break;
					}
				}
				else {
					WAWO_ASSERT(r == -1);
					int ec = socket_get_last_errno();
					if (WAWO_LIKELY(IS_ERRNO_EQUAL_WOULDBLOCK(ec))) {
						ec_o = wawo::E_CHANNEL_WRITE_BLOCK;
						break;
					}
					else if (WAWO_UNLIKELY(ec == wawo::E_EINTR)) {
						continue;
					}
					else {
						WAWO_TRACE_SOCKET_API("[wawo::net::send][#%d]send failed: %d", fd, ec);
						ec_o = ec;
						break;
					}
				}
			} while (true);

			WAWO_TRACE_SOCKET_API("[wawo::net::send][#%d]send, to send: %d, sent: %d, ec: %d", fd, len, R, ec_o);
			return R;
		}

		inline wawo::u32_t recv(SOCKET const&fd, byte_t* const buffer_o, wawo::u32_t const& size, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);

			wawo::u32_t R = 0;
			do {
				const int r = ::recv(fd, reinterpret_cast<char*>(buffer_o) + R, (int)(size - R), flag);
				if (WAWO_LIKELY(r>0)) {
					R += r;
					ec_o = wawo::OK;
					break;
				}
				else if (r == 0) {
					WAWO_TRACE_SOCKET_API("[wawo::net::recv][#%d]socket closed by remote side gracefully[detected by recv]", fd);
					ec_o = wawo::E_SOCKET_GRACE_CLOSE;
					break;
				}
				else {
					WAWO_ASSERT(r == -1);
					int ec = socket_get_last_errno();

					if (WAWO_LIKELY(IS_ERRNO_EQUAL_WOULDBLOCK(ec))) {
						ec_o = wawo::E_CHANNEL_READ_BLOCK;
						break;
					}
					else if (WAWO_UNLIKELY(ec == wawo::E_EINTR)) {
						continue;
					}
					else {
						WAWO_ASSERT(ec != wawo::OK);
						ec_o = ec;
						WAWO_TRACE_SOCKET_API("[wawo::net::recv][#%d]recv: %d", fd, ec);
						break;
					}
				}
			} while (true);

			WAWO_TRACE_SOCKET_API("[wawo::net::recv][#%d]recv bytes, %u, ec: %d", fd, R, ec_o);
			WAWO_ASSERT(R <= size);
			return R;
		}

		inline wawo::u32_t sendto(SOCKET const& fd, wawo::byte_t const* const buff, wawo::u32_t const& len, address const& addr, int& ec_o, int const& flag) {

			WAWO_ASSERT(buff != NULL);
			WAWO_ASSERT(len > 0);
			WAWO_ASSERT( !addr.is_null() );

			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));
			addr_in.sin_family = OS_DEF_family[addr.family()];
			addr_in.sin_port = addr.nport();
			addr_in.sin_addr.S_un.S_addr = addr.nipv4();

			ec_o = wawo::OK;
			u32_t sent_total = 0;
			do {
				const int sent = ::sendto(fd, reinterpret_cast<const char*>(buff), (int)len, flag, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));

				if (WAWO_LIKELY(sent > 0)) {
					WAWO_ASSERT((u32_t)sent == len);
					ec_o = wawo::OK;
					sent_total = sent;
					break;
				}

				WAWO_ASSERT(sent == -1);
				int send_ec = socket_get_last_errno();
				if (IS_ERRNO_EQUAL_WOULDBLOCK(send_ec)) {
					ec_o = wawo::E_CHANNEL_WRITE_BLOCK;
				}
				else if (send_ec == wawo::E_EINTR) {
					continue;
				}
				else {
					WAWO_TRACE_SOCKET_API("[wawo::net::sendto][#%d]send failed, error code: %d", fd, send_ec);
					ec_o = WAWO_NEGATIVE(send_ec);
				}
				break;

			} while (true);

			WAWO_TRACE_SOCKET_API("[wawo::net::sendto][#%d]sendto() == %d", fd, sent_total);
			return sent_total;
		}

		inline wawo::u32_t recvfrom(SOCKET const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag) {

			sockaddr_in addr_in;
			::memset(&addr_in, 0, sizeof(addr_in));
			socklen_t socklen = sizeof(addr_in);
			wawo::u32_t r_total;
			do {
				const int nbytes = ::recvfrom(fd, reinterpret_cast<char*>(buff_o), (int)size, flag, reinterpret_cast<sockaddr*>(&addr_in), &socklen);

				if (WAWO_LIKELY(nbytes > 0)) {
					r_total = nbytes;
					addr_o = address(addr_in);
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
					WAWO_TRACE_SOCKET_API("[wawo::net::recvfrom][#%d]recvfrom, ERROR: %d", fd, _ern);
				}

				r_total = 0;
				break;
			} while (true);

			WAWO_TRACE_SOCKET_API("[wawo::net::recvfrom][#%d]recvfrom() == %d", fd, r_total);
			return r_total;
		}

		inline int socketpair(int domain, int type, int protocol, SOCKET sv[2]) {
			if (domain != wawo::net::F_AF_INET) {
				return wawo::E_INVAL;
			}

			if (type == wawo::net::T_DGRAM) {
				if (protocol != wawo::net::P_UDP) {
					return wawo::E_INVAL;
				}
			}

			WAWO_ASSERT(protocol == P_TCP || protocol == P_UDP);

#ifdef WAWO_PLATFORM_WIN
			SOCKET listenfd = ::socket(OS_DEF_family[domain], OS_DEF_sock_type[type], OS_DEF_protocol[protocol]);
			WAWO_RETURN_V_IF_MATCH((int)wawo::E_SOCKET_ERROR, listenfd == wawo::E_INVALID_SOCKET);

			struct sockaddr_in addr_listen;
			struct sockaddr_in addr_connect;
			struct sockaddr_in addr_accept;
			::memset(&addr_listen, 0, sizeof(addr_listen));

			SOCKET connectfd;
			SOCKET acceptfd;
			socklen_t socklen = sizeof(addr_connect);

			addr_listen.sin_family = AF_INET;
			addr_listen.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
			addr_listen.sin_port = 0;
			int rt = ::bind(listenfd, reinterpret_cast<sockaddr*>(&addr_listen), sizeof(addr_listen));
			if (rt != wawo::OK) {
				goto end;
			}
			rt = ::listen(listenfd, 1);
			if (rt != wawo::OK) {
				goto end;
			}
			rt = ::getsockname(listenfd, reinterpret_cast<sockaddr*>(&addr_connect), &socklen);
			if (rt != wawo::OK) {
				goto end;
			}

			connectfd = ::socket(OS_DEF_family[domain], OS_DEF_sock_type[type], OS_DEF_protocol[protocol]);
			if (connectfd == wawo::E_INVALID_SOCKET) {
				goto end;
			}
			rt = ::connect(connectfd, reinterpret_cast<sockaddr*>(&addr_connect), sizeof(addr_connect));
			if (rt != wawo::OK) {
				goto end;
			}

			acceptfd = ::accept(listenfd, reinterpret_cast<sockaddr*>(&addr_accept), &socklen);
			if (acceptfd == wawo::E_INVALID_SOCKET) {
				WAWO_CLOSE_SOCKET(connectfd);
				goto end;
			}

			sv[0] = acceptfd;
			sv[1] = connectfd;
		end:
			WAWO_CLOSE_SOCKET(listenfd);
			return rt;
		}
#else
		if (domain != wawo::net::s_family::F_AF_UNIX)) {
			wawo::set_last_errno(wawo::E_INVAL);
			return wawo::SOCKET_ERROR;
		}
		return ::socketpair(OS_DEF_family[domain], OS_DEF_sock_type[type], OS_DEF_protocol[protocol], sv);
#endif
	}

		namespace helper {

		inline int set_keep_alive(SOCKET fd, bool onoff) {
			int optval = onoff ? 1 : 0;
			return wawo::net::socket_api::posix::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
		}
		
		inline int set_reuseaddr(SOCKET fd, bool onoff) {
			int optval = onoff ? 1 : 0;
			return wawo::net::socket_api::posix::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		}

#if WAWO_ISGNU
		inline int set_reuseport(SOCKET fd, bool onoff) {
			int optval = onoff ? 1 : 0;
			return wawo::net::socket_api::posix::setsockopt(fd, SOL_SOCKET, OPTION_REUSEPORT, &optval, sizeof(optval));
		}
#endif
		inline int set_broadcast(SOCKET fd, bool onoff) {
			int optval = onoff ? 1 : 0;
			return wawo::net::socket_api::posix::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
		}

		inline int set_nonblocking(SOCKET fd, bool onoff) {
			int rt;
#if WAWO_IS_GNU
			int mode = ::fcntl(m_fd, F_GETFL, 0);
			if (on_or_off) {
				mode |= O_NONBLOCK;
			} else {
				mode &= ~O_NONBLOCK;
			}
			rt = ::fcntl(m_fd, F_SETFL, mode);
#else
			ULONG nonBlocking = onoff ? 1: 0;
			rt = ::ioctlsocket(fd, FIONBIO, &nonBlocking);
#endif
			return rt;
		}
		inline int turnon_nonblocking(SOCKET fd) {
			return set_nonblocking(fd, true);
		}
		inline int turnoff_nonblocking(SOCKET fd) {
			return set_nonblocking(fd, false);
		}

		inline int set_nodelay(SOCKET fd, bool onoff) {
			int optval = onoff ? 1 : 0;
			return wawo::net::socket_api::posix::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
		}

		inline int turnon_nodelay(SOCKET fd) {
			return set_nodelay(fd, true);
		}
		inline int turnoff_nodelay(SOCKET fd) {
			return set_nodelay(fd, false);
		}
	}

#ifdef WAWO_IO_MODE_IOCP
	namespace iocp {
		inline SOCKET socket(int const& family, int const& type, int const& proto) {
			return ::WSASocketW( OS_DEF_family[family], OS_DEF_sock_type[type], OS_DEF_protocol[proto], NULL, 0, WSA_FLAG_OVERLAPPED);
		}
	}
#endif

#ifdef WAWO_ENABLE_WCP
#include <wawo/net/wcp.hpp>
	namespace wcp {
		inline int socket(SOCKET const& family, int const& socket_type, int const& protocol) {
			return wcp::instance()->socket(family, socket_type, protocol);
		}

		inline int connect(SOCKET const& fd, const struct sockaddr* addr, socklen_t const& length) {
			return wcp::instance()->connect(fd, addr, length);
		}

		inline int bind(SOCKET const& fd, const struct sockaddr* addr, socklen_t const& length) {
			return wcp::instance()->bind(fd, addr, length);
		}

		inline int shutdown(SOCKET const& fd, int const& flag) {
			return wcp::instance()->shutdown(fd, flag);
		}

		inline int close(SOCKET const& fd) {
			return wcp::instance()->close(fd);
		}
		inline int listen(SOCKET const& fd, int const& backlog) {
			return wcp::instance()->listen(fd, backlog);
		}

		inline int accept(SOCKET const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			return wcp::instance()->accept(fd, addr, addrlen);
		}

		inline int getsockopt(SOCKET const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len) {
			return wcp::instance()->getsockopt(fd, level, option_name, value, option_len);
		}

		inline int setsockopt(SOCKET const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_len) {
			return wcp::instance()->setsockopt(fd, level, option_name, value, option_len);
		}

		inline int getsockname(SOCKET const& fd, struct sockaddr* addr, socklen_t* addrlen) {
			return wcp::instance()->getsockname(fd, addr, addrlen);
		}

		inline u32_t send(SOCKET const& fd, byte_t const* const buffer, u32_t const& len, int& ec_o, int const& flag = 0) {
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
					if (IS_ERRNO_EQUAL_WOULDBLOCK(r)) {
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

		inline u32_t recv(SOCKET const& fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0) {

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
					if (IS_ERRNO_EQUAL_WOULDBLOCK(r)) {
						ec_o = wawo::E_CHANNEL_READ_BLOCK;
					}
					else if (WAWO_ABS(r) == EINTR) {
						continue;
					}
					else {
						ec_o = WAWO_NEGATIVE(r);
						WAWO_TRACE_SOCKET_API("[wawo::net::wcp_socket::recv][#%d]recv error, errno: %d", fd, r);
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

		inline int fcntl(SOCKET const& fd, int const cmd, ...) {
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