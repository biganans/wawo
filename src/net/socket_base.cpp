#include <wawo/task/scheduler.hpp>
#include <wawo/net/socket_base.hpp>

namespace wawo { namespace net { namespace standard_socket {
	inline int socket(int const& family, int const& socket_type, int const& protocol) {
		int rt = ::socket(family, socket_type, protocol);
		WAWO_RETURN_V_IF_MATCH(rt, rt > 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int connect(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
		int rt = ::connect(fd, addr, length);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int bind(int const& fd, const struct sockaddr* addr, socklen_t const& length) {
		int rt = ::bind(fd, addr, length);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int shutdown(int const& fd, int const& flag) {
		int rt = ::shutdown(fd, flag);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int close(int const& fd) {
		int rt = WAWO_CLOSE_SOCKET(fd);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int listen(int const& fd, int const& backlog) {
		int rt = ::listen(fd, backlog);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int accept(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
		int accepted_fd = ::accept(fd, addr, addrlen);
		WAWO_RETURN_V_IF_MATCH(accepted_fd, (accepted_fd > 0));
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int getsockopt(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len) {
		int rt = ::getsockopt(fd, level, option_name, (char*)value, option_len);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int setsockopt(int const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_len) {
		int rt = ::setsockopt(fd, level, option_name, (char*)value, option_len);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline int getsockname(int const& fd, struct sockaddr* addr, socklen_t* addrlen) {
		int rt = ::getsockname(fd, addr, addrlen);
		WAWO_RETURN_V_IF_MATCH(0, rt == 0);
		return WAWO_NEGATIVE(socket_get_last_errno());
	}

	inline u32_t send(int const& fd, byte_t const* const buffer, u32_t const& len, int& ec_o, int const& flag ) {
		WAWO_ASSERT(buffer != NULL);
		WAWO_ASSERT(len > 0);

		u32_t R = 0;

		//TRY SEND
		do {
			int r = ::send(fd, reinterpret_cast<const char*>(buffer) + R, len - R, flag);
			if ( WAWO_LIKELY(r>0) ) {
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
					ec_o = wawo::E_SOCKET_SEND_BLOCK;
					//WAWO_TRACE_SOCKET("[wawo::net::send][#%d]send blocked, error code: <%d>", fd, ec);
				}
				else if (ec == EINTR) {
					continue;
				}
				else {
					WAWO_ERR("[wawo::net::send][#%d]send failed, error code: <%d>", fd, ec);
					ec_o = WAWO_NEGATIVE(ec);
				}
				break;
			}
		} while (true);

		WAWO_TRACE_SOCKET_INOUT("[wawo::net::send][#%d]send, to send: %d, sent: %d, ec: %d", fd, len, sent_total, ec_o );
		return R;
	}

	inline u32_t recv(int const&fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag ) {
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
				WAWO_TRACE_SOCKET("[wawo::net::recv][#%d]socket closed by remote side gracefully[detected by recv]", fd);
				ec_o = wawo::E_SOCKET_GRACE_CLOSE;
				break;
			}
			else {
				WAWO_ASSERT(r == -1);
				int ec = socket_get_last_errno();
				if (IS_ERRNO_EQUAL_WOULDBLOCK(ec)) {
					//WAWO_TRACE_SOCKET("[wawo::net::recv][#%d]recv blocked, block code: <%d>", fd, ec);
					ec_o = wawo::E_SOCKET_RECV_BLOCK;
				}
				else if (ec == EINTR) {
					continue;
				}
				else {
					WAWO_ASSERT(ec != wawo::OK);
					ec_o = WAWO_NEGATIVE(ec);
					WAWO_ERR("[wawo::net::recv][#%d]recv error, errno: %d", fd, ec);
				}
				break;
			}
		} while (true);

		WAWO_TRACE_SOCKET_INOUT("[wawo::net::recv][#%d]recv bytes, %d", fd, r_total);
		return r_total;
	}

	inline u32_t sendto(int const& fd, wawo::byte_t const* const buff, wawo::u32_t const& len, const wawo::net::address& addr, int& ec_o, int const& flag ) {

		WAWO_ASSERT(buff != NULL);
		WAWO_ASSERT(len > 0);

		sockaddr_in addr_in;
		ec_o = wawo::OK;

		WAWO_ASSERT(addr.get_netsequence_ulongip() != 0);

		addr_in.sin_family = AF_INET;
		addr_in.sin_addr.s_addr = addr.get_netsequence_ulongip();
		addr_in.sin_port = addr.get_netsequence_port();

		u32_t sent_total = 0;

		do {
			int sent = ::sendto(fd, reinterpret_cast<const char*>(buff), len, flag, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));

			if ( WAWO_LIKELY(sent>0)) {
				WAWO_ASSERT( (u32_t)sent == len );
				ec_o = wawo::OK;
				sent_total = sent;
				break;
			}

			WAWO_ASSERT(sent == -1);
			int send_ec = socket_get_last_errno();
			if (IS_ERRNO_EQUAL_WOULDBLOCK(send_ec)) {
				ec_o = wawo::E_SOCKET_SEND_BLOCK;
				//WAWO_TRACE_SOCKET("[wawo::net::sendto][#%d]send blocked, error code: <%d>, no retry", fd, send_ec);
			}
			else if (send_ec == EINTR) {
				continue;
			}
			else {
				WAWO_ERR("[wawo::net::sendto][#%d]send failed, error code: <%d>", fd, send_ec);
				ec_o = WAWO_NEGATIVE(send_ec);
			}
			break;

		} while (true);

		WAWO_TRACE_SOCKET_INOUT("[wawo::net::sendto][#%d]sendto() == %d", fd, sent_total);
		return sent_total;
	}

	inline u32_t recvfrom(int const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag ) {

		sockaddr_in addr_in;
		u32_t r_total;
		do {
			socklen_t socklen = sizeof(addr_in);
			int nbytes = ::recvfrom(fd, reinterpret_cast<char*>(buff_o), size, flag, reinterpret_cast<sockaddr*>(&addr_in), &socklen);

			if ( WAWO_LIKELY(nbytes>0)) {
				r_total = nbytes;
				addr_o.set_netsequence_port((addr_in.sin_port));
				addr_o.set_netsequence_ulongip(addr_in.sin_addr.s_addr);
				ec_o = wawo::OK;
				break;
			}

			WAWO_ASSERT(nbytes==-1);
			int _ern = socket_get_last_errno();
			if (IS_ERRNO_EQUAL_WOULDBLOCK(_ern)) {
				ec_o = E_SOCKET_RECV_BLOCK;
				//WAWO_TRACE_SOCKET("[wawo::net::recvfrom][#%d]recvfrom EWOULDBLOCK", fd);
			} else if(_ern == EINTR) {
				continue;
			}
			else {
				ec_o = WAWO_NEGATIVE(_ern);
				WAWO_ERR("[wawo::net::recvfrom][#%d]recvfrom, ERROR: %d", fd, _ern);
			}

			r_total =0 ;
			break;
		} while (true);

		WAWO_TRACE_SOCKET_INOUT("[wawo::net::recvfrom][#%d]recvfrom() == %d", fd, r_total);
		return r_total;
	}
}}}


#include <wawo/net/wcp.hpp>
namespace wawo { namespace net { namespace wcp_socket {
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
			int r = wcp::instance()->send(fd, (buffer+sent_total), (len-sent_total), flag);
			if ( WAWO_LIKELY(r>0)) {
				ec_o = wawo::OK;
				sent_total += r;
				if (sent_total == len) {
					break;
				}
			}
			else {
				WAWO_ASSERT(r < 0 );
				if (IS_ERRNO_EQUAL_WOULDBLOCK(WAWO_ABS(r))) {
					ec_o = wawo::E_SOCKET_SEND_BLOCK;
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
					ec_o = wawo::E_SOCKET_RECV_BLOCK;
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

}}}

namespace wawo { namespace net {

	void socket_base::_socket_fn_init() {
#ifdef WAWO_ENABLE_WCP
		if (m_protocol == P_WCP) {
			m_fn_socket = wawo::net::wcp_socket::socket;
			m_fn_connect = wawo::net::wcp_socket::connect;
			m_fn_bind = wawo::net::wcp_socket::bind;
			m_fn_shutdown = wawo::net::wcp_socket::shutdown;
			m_fn_close = wawo::net::wcp_socket::close;
			m_fn_listen = wawo::net::wcp_socket::listen;
			m_fn_accept = wawo::net::wcp_socket::accept;
			m_fn_getsockopt = wawo::net::wcp_socket::getsockopt;
			m_fn_setsockopt = wawo::net::wcp_socket::setsockopt;
			m_fn_getsockname = wawo::net::wcp_socket::getsockname;
			m_fn_send = wawo::net::wcp_socket::send;
			m_fn_recv = wawo::net::wcp_socket::recv;
			m_fn_sendto = wawo::net::standard_socket::sendto;
			m_fn_recvfrom = wawo::net::standard_socket::recvfrom;
		} else {
#endif
			m_fn_socket = wawo::net::standard_socket::socket;
			m_fn_connect = wawo::net::standard_socket::connect;
			m_fn_bind = wawo::net::standard_socket::bind;
			m_fn_shutdown = wawo::net::standard_socket::shutdown;
			m_fn_close = wawo::net::standard_socket::close;
			m_fn_listen = wawo::net::standard_socket::listen;
			m_fn_accept = wawo::net::standard_socket::accept;
			m_fn_getsockopt = wawo::net::standard_socket::getsockopt;
			m_fn_setsockopt = wawo::net::standard_socket::setsockopt;
			m_fn_getsockname = wawo::net::standard_socket::getsockname;
			m_fn_send = wawo::net::standard_socket::send;
			m_fn_recv = wawo::net::standard_socket::recv;
			m_fn_sendto = wawo::net::standard_socket::sendto;
			m_fn_recvfrom = wawo::net::standard_socket::recvfrom;
#ifdef WAWO_ENABLE_WCP
		}
#endif
	}

		socket_base::socket_base(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_sm(sm),
			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),

			m_addr(addr),
			m_bind_addr(),

			m_fd(fd),
			m_sbc(sbc)
		{
			_socket_fn_init();

			WAWO_ASSERT(fd > 0);

			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);

			WAWO_TRACE_SOCKET("[socket][%s]socket::socket(), address: %p", info().to_lencstr().cstr, this);
		}

		socket_base::socket_base(s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_sm(SM_NONE),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),
			m_addr(),
			m_bind_addr(),

			m_fd(-1),
			m_sbc(socket_buffer_cfgs[BT_MEDIUM])
		{
			_socket_fn_init();

			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);
			WAWO_TRACE_SOCKET("[socket][%s]socket::socket(), dummy socket, address: %p", info().to_lencstr().cstr, this);
		}

		socket_base::socket_base(socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_sm(SM_NONE),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),
			m_addr(),
			m_bind_addr(),

			m_fd(-1),
			m_sbc(sbc)
		{
			_socket_fn_init();
			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);
			WAWO_TRACE_SOCKET("[socket][%s]socket::socket(), dummy socket, address: %p", info().to_lencstr().cstr , this);
		}

		socket_base::~socket_base() {
			WAWO_TRACE_SOCKET("[socket][%s]socket::~socket(),address: %p", info().to_lencstr().cstr, this);
		}

		address socket_base::local_addr() const {
			if (is_listener()) {
				return m_bind_addr;
			}

			struct sockaddr_in addr_in;
			socklen_t addr_in_length = sizeof(addr_in);

			int rt = m_fn_getsockname(m_fd, (struct sockaddr*) &addr_in, &addr_in_length);
			if (rt == wawo::OK) {
				WAWO_ASSERT(addr_in.sin_family == AF_INET);
				return address(addr_in);
			}
			return address();
		}

		int socket_base::open() {

			WAWO_ASSERT(m_fd == -1);

			int soFamily;
			int soType;
			int soProtocol;

			if (m_family == F_PF_INET) {
				soFamily = PF_INET;
			}
			else if (m_family == F_AF_INET) {
				soFamily = AF_INET;
			}
			else if (m_family == F_AF_UNIX) {
				soFamily = AF_UNIX;
			}
			else {
				return wawo::E_SOCKET_INVALID_FAMILY;
			}

			if (m_type == T_STREAM) {
				soType = SOCK_STREAM;
			}
			else if (m_type == T_DGRAM) {
				soType = SOCK_DGRAM;
			}
			else if (m_type == T_RAW) {
				soType = SOCK_RAW;
			} else {
				char message[128] = { 0 };
				snprintf(message, 128, "unsupported socket type, %d", m_type);
				WAWO_ASSERT(!"unsupported socket type", "type: %d", m_type);

				return wawo::E_SOCKET_INVALID_TYPE;
			}

			if (m_protocol == P_TCP) {
				soProtocol = IPPROTO_TCP;
			} else if (m_protocol == P_UDP
				|| m_protocol == P_WCP
				) {
				soProtocol = IPPROTO_UDP;
			} else if (m_protocol == P_ICMP) {
				soProtocol = IPPROTO_ICMP;
			} else {
				WAWO_ASSERT(!"unsupported ip protocol", "pro: %d", m_protocol);
				return wawo::E_SOCKET_INVALID_TYPE;
			}

			int fd = m_fn_socket(soFamily, soType, soProtocol);
			if (fd < 0) {
				WAWO_ERR("[socket][%s]socket::socket() failed, %d", info().to_lencstr().cstr);
				return fd;
			}
			WAWO_ASSERT( fd>0 );

			m_fd = fd;

			WAWO_TRACE_SOCKET("[socket][%s]socket::socket() ok", info().to_lencstr().cstr );

			int rt = set_options(m_option);

			if (rt != wawo::OK) {
				WAWO_ERR("[socket][%s]socket::_set_options() failed", info().to_lencstr().cstr );
				return rt;
			}

			if (m_protocol == P_UDP) {
				return wawo::OK;
			}

#ifdef _DEBUG
			u32_t tmp_size;
			get_snd_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket][%s]current snd buffer size: %d", info().to_lencstr().cstr, tmp_size);
#endif
			rt = set_snd_buffer_size(m_sbc.snd_size);
			if (rt != wawo::OK) {
				WAWO_ERR("[socket][%s]socket::set_snd_buffer_size(%d) failed", info().to_lencstr().cstr, m_sbc.snd_size, WAWO_NEGATIVE(socket_get_last_errno()));
				return rt;
			}
#ifdef _DEBUG
			get_snd_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket][%s]current snd buffer size: %d", info().to_lencstr().cstr, tmp_size);
#endif

#ifdef _DEBUG
			get_rcv_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket][%s]current rcv buffer size: %d", info().to_lencstr().cstr, tmp_size);
#endif
			rt = set_rcv_buffer_size(m_sbc.rcv_size);
			if (rt != wawo::OK) {
				WAWO_ERR("[socket][%s]socket::set_rcv_buffer_size(%d) failed", info().to_lencstr().cstr , m_sbc.rcv_size, WAWO_NEGATIVE(socket_get_last_errno()));
				return rt;
			}
#ifdef _DEBUG
			get_rcv_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket][%s]current rcv buffer size: %d", info().to_lencstr().cstr, tmp_size);
#endif
			return wawo::OK;
		}

		int socket_base::close() {
			int close_rt = m_fn_close(m_fd);
			WAWO_ASSERT(close_rt == wawo::OK);

			if (close_rt == 0) {
				WAWO_TRACE_SOCKET("[socket][%s]socket close", info().to_lencstr().cstr );
			} else {
				WAWO_WARN("[socket][%s]socket close, close_rt: %d, close_ec: %d", info().to_lencstr().cstr , close_rt, WAWO_NEGATIVE(socket_get_last_errno()));
			}
			return close_rt;
		}

		int socket_base::shutdown(int const& flag) {

			WAWO_ASSERT(is_data_socket());

			const char* shutdown_flag_str[3] = {
				"SHUT_RD",
				"SHUT_WR",
				"SHUT_RDWR"
			};

			int shutrt = m_fn_shutdown(m_fd, flag);
			WAWO_TRACE_SOCKET("[socket][%s]shutdown(%s)", info().to_lencstr().cstr, shutdown_flag_str[flag]);
			if(shutrt != 0) {
				WAWO_WARN("[socket][%s]shutdown(%s), shut_rt: %d", info().to_lencstr().cstr, shutdown_flag_str[flag], shutrt);
			}
			return shutrt;
		}

		int socket_base::bind(const address& addr) {

			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_bind_addr.is_null());

			short soFamily;
			if (m_family == F_PF_INET) {
				soFamily = PF_INET;
			}
			else if (m_family == F_AF_INET) {
				soFamily = AF_INET;
			}
			else if (m_family == F_AF_UNIX) {
				soFamily = AF_UNIX;
			}
			else {
				return wawo::E_SOCKET_INVALID_FAMILY;
			}

			if (m_protocol == wawo::net::P_WCP) {
				int rt = reuse_addr();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

				rt = reuse_port();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
			}

			sockaddr_in addr_in;
			addr_in.sin_family = soFamily;
			addr_in.sin_port = addr.get_netsequence_port();
			addr_in.sin_addr.s_addr = addr.get_netsequence_ulongip();

			m_bind_addr = addr;
			return m_fn_bind(m_fd , reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));
		}

		int socket_base::listen(int const& backlog) {
			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_fd>0);
			int listenrt;

			if (m_protocol == P_UDP) {
				listenrt = wawo::OK;
			}
			else {
				listenrt = m_fn_listen(m_fd, backlog);
			}
			WAWO_RETURN_V_IF_NOT_MATCH(listenrt, listenrt == wawo::OK);
			m_sm = SM_LISTENER;
			return wawo::OK;
		}

		int socket_base::accept(address& addr) {
			sockaddr_in addrin;
			socklen_t addrlen = sizeof(addrin);

			int fd = m_fn_accept(m_fd, reinterpret_cast<sockaddr*>(&addrin), &addrlen);
			WAWO_RETURN_V_IF_NOT_MATCH(fd, fd < 0);

			addr.set_netsequence_ulongip(addrin.sin_addr.s_addr);
			addr.set_netsequence_port(addrin.sin_port);
			return fd;
		}
	
		int socket_base::connect(wawo::net::address const& addr ) {
			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_addr.is_null());

			short soFamily;
			if (m_family == F_PF_INET) {
				soFamily = PF_INET;
			}
			else if (m_family == F_AF_INET) {
				soFamily = AF_INET;
			}
			else if (m_family == F_AF_UNIX) {
				soFamily = AF_UNIX;
			}
			else {
				return wawo::E_SOCKET_INVALID_FAMILY;
			}

			m_sm = SM_ACTIVE;
			sockaddr_in addr_in;
			addr_in.sin_family = soFamily;
			addr_in.sin_port = addr.get_netsequence_port();
			addr_in.sin_addr.s_addr = addr.get_netsequence_ulongip();
			int socklength = sizeof(addr_in);
			m_addr = addr;

			return m_fn_connect(m_fd, reinterpret_cast<sockaddr*>(&addr_in), socklength);
		}

		int socket_base::turnoff_nodelay() {
			lock_guard<spin_mutex> _lg(m_option_mutex);

			if (!(m_option & OPTION_NODELAY)) {
				return wawo::OK;
			}

			return set_options((m_option & ~OPTION_NODELAY));
		}

		int socket_base::turnon_nodelay() {
			lock_guard<spin_mutex> _lg(m_option_mutex);

			if ((m_option & OPTION_NODELAY)) {
				return wawo::OK;
			}

			return set_options((m_option | OPTION_NODELAY));
		}

		int socket_base::turnon_nonblocking() {
			lock_guard<spin_mutex> _lg(m_option_mutex);

			if ((m_option & OPTION_NON_BLOCKING)) {
				return wawo::OK;
			}

			int op = set_options(m_option | OPTION_NON_BLOCKING);
			return op;
		}

		int socket_base::turnoff_nonblocking() {
			lock_guard<spin_mutex> _lg(m_option_mutex);
			if (!(m_option & OPTION_NON_BLOCKING)) {
				return true;
			}
			return set_options(m_option & ~OPTION_NON_BLOCKING);
		}

		int socket_base::set_snd_buffer_size(u32_t const& size) {
			WAWO_ASSERT(size >= SOCK_SND_MIN_SIZE && size <= SOCK_SND_MAX_SIZE);
			WAWO_ASSERT(m_fd > 0);

			int rt = m_fn_setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size));
			if (wawo::OK == rt) {
				return wawo::OK;
			}

			WAWO_ERR("[socket][%s]setsockopt(SO_SNDBUF) == %d failed, error code: %d", info().to_lencstr().cstr , size, rt); \
			return rt;
		}

		int socket_base::get_snd_buffer_size(u32_t& size) const {
			WAWO_ASSERT(m_fd > 0);

			int iBufSize = 0;
			socklen_t opt_length = sizeof(u32_t);
			int rt = m_fn_getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&iBufSize, &opt_length);

			if (rt == wawo::OK) {
				size = iBufSize;
				return wawo::OK;
			}

			WAWO_ERR("[socket][%s]getsockopt(SO_SNDBUF) failed, error code: %d", info().to_lencstr().cstr, rt);
			return rt;
		}

		int socket_base::get_left_snd_queue(u32_t& size) const {
#if WAWO_ISGNU
			if (m_fd <= 0) {
				size = 0;
				return -1;
			}

			int rt = ::ioctl(m_fd, TIOCOUTQ, &size);

			WAWO_RETURN_V_IF_MATCH(rt, rt == 0);
			return socket_get_last_errno();
#else
			(void)size;
			WAWO_THROW("this operation does not supported on windows");
#endif
		}

		int socket_base::get_left_rcv_queue(u32_t& size) const {
			if (m_fd <= 0) {
				size = 0;
				return -1;
			}
#if WAWO_ISGNU
			int rt = ioctl(m_fd, FIONREAD, size);
#else
			u_long ulsize;
			int rt = ::ioctlsocket(m_fd, FIONREAD, &ulsize);
			if (rt == 0) size = ulsize & 0xFFFFFFFF;
#endif

			WAWO_RETURN_V_IF_MATCH(rt, rt == 0);
			return socket_get_last_errno();
		}

		int socket_base::set_rcv_buffer_size(u32_t const& size) {
			WAWO_ASSERT(size >= SOCK_RCV_MIN_SIZE && size <= SOCK_RCV_MAX_SIZE);
			WAWO_ASSERT(m_fd > 0);

			int rt = m_fn_setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size));
			if (wawo::OK == rt) {
				return wawo::OK;
			}

			WAWO_ERR("[socket][%s]setsockopt(SO_RCVBUF) == %d failed, error code: %d", info().to_lencstr().cstr, size, rt);
			return rt;
		}

		int socket_base::get_rcv_buffer_size(u32_t& size) const {
			WAWO_ASSERT(m_fd > 0);

			int iBufSize = 0;
			socklen_t opt_length = sizeof(u32_t);

			int rt = m_fn_getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&iBufSize, &opt_length);
			if (rt == wawo::OK ) {
				size = iBufSize;
				return wawo::OK;
			}

			WAWO_ERR("[socket][%s]getsockopt(SO_RCVBUF) failed, error code: %d", info().to_lencstr().cstr , rt);
			return rt;
		}

		int socket_base::get_linger(bool& on_off, int& linger_t) const {
			WAWO_ASSERT(m_fd > 0);

			struct linger soLinger;
			socklen_t opt_length = sizeof(soLinger);

			int rt = m_fn_getsockopt(m_fd, SOL_SOCKET, SO_LINGER, (char*)&soLinger, &opt_length);

			if (rt == 0) {
				on_off = (soLinger.l_onoff != 0);
				linger_t = soLinger.l_linger;
			}

			return rt;
		}

		int socket_base::set_linger(bool const& on_off, int const& linger_t /* in seconds */) {
			struct linger soLinger;
			WAWO_ASSERT(m_fd > 0);

			soLinger.l_onoff = on_off;
			soLinger.l_linger = linger_t;

			return m_fn_setsockopt(m_fd, SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
		}

		int socket_base::set_options(int const& options) {

			if (m_fd<0) {
				WAWO_WARN("[socket][%s]socket set options failed, invalid state", info().to_lencstr().cstr );
				return wawo::E_INVALID_STATE;
			}

			int optval;

			int ret = -2;
			bool should_set;
			if (m_type == T_DGRAM) {

				should_set = ((m_option&OPTION_BROADCAST) && ((options&OPTION_BROADCAST) == 0)) ||
					(((m_option&OPTION_BROADCAST) == 0) && ((options&OPTION_BROADCAST)));

				if (should_set) {
					optval = (options & OPTION_BROADCAST) ? 1 : 0;
					ret = m_fn_setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_BROADCAST;
						}
						else {
							m_option &= ~OPTION_BROADCAST;
						}
					}
					else {
						WAWO_ERR("[socket][%s]socket set socket::OPTION_BROADCAST failed, errno: %d", info().to_lencstr().cstr, ret );
						return ret;
					}
				}
			}

			{
				should_set = (((m_option&OPTION_REUSEADDR) && ((options&OPTION_REUSEADDR) == 0)) ||
					(((m_option&OPTION_REUSEADDR) == 0) && ((options&OPTION_REUSEADDR))));

				if (should_set) {

					optval = (options & OPTION_REUSEADDR) ? 1 : 0;
					ret = m_fn_setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_REUSEADDR;
						}
						else {
							m_option &= ~OPTION_REUSEADDR;
						}
					}
					else {
						WAWO_ERR("[socket][%s]socket set socket::OPTION_REUSEADDR failed, errno: %d", info().to_lencstr().cstr, ret );
						return ret;
					}
				}
			}

#if WAWO_ISGNU
			{

				should_set = (((m_option&OPTION_REUSEPORT) && ((options&OPTION_REUSEPORT) == 0)) ||
					(((m_option&OPTION_REUSEPORT) == 0) && ((options&OPTION_REUSEPORT))));

				if (should_set) {

					optval = (options & OPTION_REUSEPORT) ? 1 : 0;
					ret = m_fn_setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_REUSEPORT;
						}
						else {
							m_option &= ~OPTION_REUSEPORT;
						}
					}
					else {
						WAWO_ERR("[socket][%s]socket set socket::OPTION_REUSEPORT failed, errno: %d", info().to_lencstr().cstr, ret);
						return ret;
					}
				}
			}
#endif

			bool _nonblocking_should_set = (((m_option&OPTION_NON_BLOCKING) && ((options&OPTION_NON_BLOCKING) == 0)) ||
				(((m_option&OPTION_NON_BLOCKING) == 0) && ((options&OPTION_NON_BLOCKING))));

			optval = (options & OPTION_NON_BLOCKING) ? 1 : 0;

			if (m_protocol == P_WCP) {
				if (_nonblocking_should_set) {
					int mode = wcp_socket::fcntl(m_fd, wcp_socket::WCP_F_GETFL, 0);
					if (optval == 1) {
						mode |= WCP_O_NONBLOCK;
					}
					else {
						mode &= WCP_O_NONBLOCK;
					}

					ret = wcp_socket::fcntl(m_fd, wcp_socket::WCP_F_SETFL, mode);
				}
			}
			else {

#if WAWO_ISGNU
				if (_nonblocking_should_set) {
					int mode = ::fcntl(m_fd, F_GETFL, 0);
					if (optval == 1) {
						mode |= O_NONBLOCK;
					}
					else {
						mode &= ~O_NONBLOCK;
					}
					ret = ::fcntl(m_fd, F_SETFL, mode);
				}
#elif WAWO_ISWIN
				// FORBIDDEN NON-BLOCKING -> SET nonBlocking to 0
				if (_nonblocking_should_set) {
					DWORD nonBlocking = (options & OPTION_NON_BLOCKING) ? 1 : 0;
					ret = ioctlsocket(m_fd, FIONBIO, &nonBlocking);
				}
#else
	#error
#endif

			}

			if (_nonblocking_should_set) {
				if (ret == 0) {
					if (optval == 1) {
						m_option |= OPTION_NON_BLOCKING;
					}
					else {
						m_option &= ~OPTION_NON_BLOCKING;
					}
				}
				else {
					WAWO_ERR("[socket][%s]socket set socket::OPTION_NON_BLOCKING failed, errno: %d", info().to_lencstr().cstr, ret );
					return ret;
				}
			}

			if (m_type == T_STREAM) {

				should_set = (((m_option&OPTION_NODELAY) && ((options&OPTION_NODELAY) == 0)) ||
					(((m_option&OPTION_NODELAY) == 0) && ((options&OPTION_NODELAY))));

				if (should_set) {
					optval = (options & OPTION_NODELAY) ? 1 : 0;
					ret = m_fn_setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_NODELAY;
						}
						else {
							m_option &= ~OPTION_NODELAY;
						}
					}
					else {
						WAWO_ERR("[socket][%s]socket set socket::OPTION_NODELAY failed, errno: %d", info().to_lencstr().cstr, ret );
						return ret;
					}
				}
			}

			return wawo::OK;
		}

		int socket_base::turnon_keep_alive() {
			int keepalive = 1;
			return m_fn_setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		}

		int socket_base::turnoff_keep_alive() {
			int keepalive = 0;
			return m_fn_setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		}

		int socket_base::set_keep_alive_vals(keep_alive_vals const& vals) {

			if (m_protocol == P_WCP) {
				//@todo
				return wawo::OK;
			}

			if (vals.onoff == 0) {
				return turnoff_keep_alive();
			}

#if WAWO_ISWIN
			DWORD dwBytesRet;
			struct tcp_keepalive alive;
			::memset(&alive, 0, sizeof(alive));

			alive.onoff = 1;
			if (vals.idle == 0) {
				alive.keepalivetime = WAWO_DEFAULT_KEEPALIVE_IDLETIME;
			}
			else {
				alive.keepalivetime = vals.idle;
			}

			if (vals.interval == 0) {
				alive.keepaliveinterval = WAWO_DEFAULT_KEEPALIVE_INTERVAL;
			}
			else {
				alive.keepaliveinterval = vals.idle;
			}

			int rt = ::WSAIoctl(m_fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL);
			return rt;
#elif WAWO_ISGNU
			int rt = turnon_keep_alive();
			WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);

			if (vals.idle != 0) {
				i32_t idle = (vals.idle / 1000);
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
				WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);
			}
			if (vals.interval != 0) {
				i32_t interval = (vals.interval / 1000);
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
				WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);
			}
			if (vals.probes != 0) {
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &vals.probes, sizeof(vals.probes));
				WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);
			}
			return wawo::OK;
#else
		#error
#endif
		}

		int socket_base::get_keep_alive_vals(keep_alive_vals& vals) {

			if (m_protocol == P_WCP) {
				//@TODO
				return wawo::OK;
			}

#if WAWO_ISWIN
			(void)vals;
			WAWO_THROW("not supported");
#elif WAWO_ISGNU
			keep_alive_vals _vals;
			int rt;
			socklen_t length;
			rt = m_fn_getsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &_vals.idle, &length);
			WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);
			rt = m_fn_getsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &_vals.interval, &length);
			WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);
			rt = m_fn_getsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &_vals.probes, &length);
			WAWO_RETURN_V_IF_NOT_MATCH(socket_get_last_errno(), rt == 0);

			_vals.idle = _vals.idle * 1000;
			_vals.interval = _vals.interval * 1000;
			vals = _vals;
			return wawo::OK;
#else
	#error
#endif
		}

		int socket_base::get_tos(u8_t& tos) const {
			u8_t _tos;
			socklen_t length;

			int rt = m_fn_getsockopt(m_fd, IPPROTO_IP, IP_TOS, (char*)&_tos, &length);
			WAWO_RETURN_V_IF_NOT_MATCH(wawo::socket_get_last_errno(), rt == 0);
			tos = IPTOS_TOS(_tos);
			return rt;
		}

		int socket_base::set_tos(u8_t const& tos) {
			WAWO_ASSERT(m_fd>0);
			u8_t _tos = IPTOS_TOS(tos) | 0xe0;
			return m_fn_setsockopt(m_fd, IPPROTO_IP, IP_TOS, (char*)&_tos, sizeof(_tos));
		}

		int socket_base::reuse_addr() {
			return set_options(m_option | OPTION_REUSEADDR);
		}
		int socket_base::reuse_port() {
			return set_options(m_option | OPTION_REUSEPORT);
		}

		u32_t socket_base::sendto(wawo::byte_t const* const buffer, wawo::u32_t const& len, const wawo::net::address& addr, int& ec_o, int const& flag) {

			WAWO_ASSERT(!"TOCHECK FOR WCP");

			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);
			ec_o = wawo::OK;

			//if (m_wflag&SHUTDOWN_WR) {
			//	ec_o = wawo::E_SOCKET_WR_SHUTDOWN_ALREADY;
			//	return 0;
			//}

			//if (m_state == S_CONNECTED) {
			//	(void)addr;
#ifdef _DEBUG
			//	WAWO_ASSERT( addr == m_addr );
#endif
			//	return m_fn_send(m_fd, buffer, len, ec_o, flag);
			//}

			return m_fn_sendto(m_fd, buffer, len, addr, ec_o, flag);
		}

		u32_t socket_base::recvfrom(byte_t* const buffer_o, wawo::u32_t const& size, address& addr_o, int& ec_o) {
			ec_o = wawo::OK;
			WAWO_ASSERT(!"TOCHECK FOR WCP");
			/*
			if (m_rflag&SHUTDOWN_RD) {
				ec_o = wawo::E_SOCKET_RD_SHUTDOWN_ALREADY;
				return 0;
			}

			if (m_state == S_CONNECTED) {
#ifdef _DEBUG
				WAWO_ASSERT(!m_addr.is_null());
#endif
				addr_o = m_addr;
				return m_fn_recv(m_fd, buffer_o, size, ec_o, 0);
			}
			*/
			return m_fn_recvfrom(m_fd, buffer_o, size, addr_o, ec_o,0);
		}

		u32_t socket_base::send(byte_t const* const buffer, u32_t const& length, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(length > 0);

			return m_fn_send(m_fd, buffer, length, ec_o, flag);
		}

		u32_t socket_base::recv(byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);

			return m_fn_recv(m_fd, buffer_o, size, ec_o, flag);
		}
}}