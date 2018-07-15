#include <wawo/task/scheduler.hpp>
#include <wawo/net/socket_base.hpp>

namespace wawo { namespace net {

	void socket_base::_socket_fn_init() {
#ifdef WAWO_ENABLE_WCP
		if (m_protocol == P_WCP) {
			m_fn_socket = wawo::net::api::wcp::socket;
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
#ifdef WAWO_IO_MODE_IOCP
			m_fn_socket = wawo::net::socket_api::iocp::socket;
#else
			m_fn_socket = wawo::net::socket_api::posix::socket;
#endif
			m_fn_connect = wawo::net::socket_api::posix::connect;
			m_fn_bind = wawo::net::socket_api::posix::bind;
			m_fn_shutdown = wawo::net::socket_api::posix::shutdown;
			m_fn_close = wawo::net::socket_api::posix::close;
			m_fn_listen = wawo::net::socket_api::posix::listen;
			m_fn_accept = wawo::net::socket_api::posix::accept;
			m_fn_getsockopt = wawo::net::socket_api::posix::getsockopt;
			m_fn_setsockopt = wawo::net::socket_api::posix::setsockopt;
			m_fn_getsockname = wawo::net::socket_api::posix::getsockname;
			m_fn_send = wawo::net::socket_api::posix::send;
			m_fn_recv = wawo::net::socket_api::posix::recv;
			m_fn_sendto = wawo::net::socket_api::posix::sendto;
			m_fn_recvfrom = wawo::net::socket_api::posix::recvfrom;
#ifdef WAWO_ENABLE_WCP
		}
#endif
	}

		socket_base::socket_base(SOCKET const& fd, address const& laddr, address const& raddr, socket_mode const& sm, socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_fd(fd),
			m_sm(sm),
			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),

			m_laddr(laddr),
			m_raddr(raddr),

			m_sbc(sbc)
		{
			WAWO_ASSERT(family < F_AF_UNSPEC);
			WAWO_ASSERT(sockt < T_UNKNOWN);
			WAWO_ASSERT(proto < P_UNKNOWN);

			_socket_fn_init();
			WAWO_ASSERT(fd > 0);
			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);

			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::socket_base(), new connected address: %p", info().to_stdstring().c_str(), this);
		}

		socket_base::socket_base(s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_fd(wawo::E_INVALID_SOCKET),
			m_sm(SM_NONE),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),
			m_laddr(),
			m_raddr(),

			m_sbc(socket_buffer_cfgs[BT_MEDIUM])
		{
			WAWO_ASSERT(family < F_AF_UNSPEC);
			WAWO_ASSERT(sockt < T_UNKNOWN);
			WAWO_ASSERT(proto < P_UNKNOWN);

			_socket_fn_init();

			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);
			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::socket_base(), dummy socket, address: %p", info().to_stdstring().c_str(), this);
		}

		socket_base::socket_base(socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt) :
			m_fd(wawo::E_INVALID_SOCKET),
			m_sm(SM_NONE),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),
			m_option(opt),
			m_laddr(),
			m_raddr(),

			m_sbc(sbc)
		{
			WAWO_ASSERT(family < F_AF_UNSPEC);
			WAWO_ASSERT(sockt < T_UNKNOWN);
			WAWO_ASSERT(proto < P_UNKNOWN);

			_socket_fn_init();
			WAWO_ASSERT(m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE);
			WAWO_ASSERT(m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE);
			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::socket_base(), dummy socket, address: %p", info().to_stdstring().c_str() , this);
		}

		socket_base::~socket_base() {
			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::~socket_base(),address: %p", info().to_stdstring().c_str(), this);
		}

		int socket_base::open() {
			WAWO_ASSERT(m_fd == -1);
			m_fd = m_fn_socket(m_family, m_type, m_protocol);
			if (m_fd<0) {
				WAWO_ERR("[socket_base][%s]socket::socket() failed, %d", info().to_stdstring().c_str(), wawo::socket_get_last_errno() );
				return (int)m_fd;
			}
			WAWO_ASSERT(m_fd>0 );

			WAWO_TRACE_SOCKET("[socket_base][%s]socket::socket() ok", info().to_stdstring().c_str() );
			int rt = set_options(m_option);

			if (rt != wawo::OK) {
				WAWO_ERR("[socket_base][%s]socket::_set_options() failed", info().to_stdstring().c_str() );
				return rt;
			}

			if (m_protocol == P_UDP) {
				return wawo::OK;
			}

#ifdef _DEBUG
			u32_t tmp_size;
			get_snd_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket_base][%s]current snd buffer size: %d", info().to_stdstring().c_str(), tmp_size);
#endif
			rt = set_snd_buffer_size(m_sbc.snd_size);
			if (rt != wawo::OK) {
				WAWO_ERR("[socket_base][%s]socket::set_snd_buffer_size(%d) failed", info().to_stdstring().c_str(), m_sbc.snd_size, socket_get_last_errno());
				return rt;
			}
#ifdef _DEBUG
			get_snd_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket_base][%s]current snd buffer size: %d", info().to_stdstring().c_str(), tmp_size);
#endif

#ifdef _DEBUG
			get_rcv_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket_base][%s]current rcv buffer size: %d", info().to_stdstring().c_str(), tmp_size);
#endif
			rt = set_rcv_buffer_size(m_sbc.rcv_size);
			if (rt != wawo::OK) {
				WAWO_ERR("[socket_base][%s]socket::set_rcv_buffer_size(%d) failed", info().to_stdstring().c_str() , m_sbc.rcv_size, socket_get_last_errno());
				return rt;
			}
#ifdef _DEBUG
			get_rcv_buffer_size(tmp_size);
			WAWO_TRACE_SOCKET("[socket_base][%s]current rcv buffer size: %d", info().to_stdstring().c_str(), tmp_size);
#endif
			return wawo::OK;
		}

		int socket_base::close() {
			int close_rt = m_fn_close(m_fd);
			WAWO_ASSERT(close_rt == wawo::OK);

			if (close_rt == 0) {
				WAWO_TRACE_SOCKET("[socket_base][%s]socket close", info().to_stdstring().c_str() );
			} else {
				WAWO_WARN("[socket_base][%s]socket close, close_rt: %d, close_ec: %d", info().to_stdstring().c_str() , close_rt, socket_get_last_errno());
			}
			return close_rt;
		}

		int socket_base::shutdown(int const& flag) {

			WAWO_ASSERT(!is_listener());
			const char* shutdown_flag_str[3] = {
				"SHUT_RD",
				"SHUT_WR",
				"SHUT_RDWR"
			};

			int shutrt = m_fn_shutdown(m_fd, flag);
			WAWO_TRACE_SOCKET("[socket_base][%s]shutdown(%s)", info().to_stdstring().c_str(), shutdown_flag_str[flag]);
			if(shutrt != 0) {
				WAWO_WARN("[socket_base][%s]shutdown(%s), shut_rt: %d", info().to_stdstring().c_str(), shutdown_flag_str[flag], shutrt);
			}
			return shutrt;
		}

		int socket_base::bind(address const& addr) {

			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_laddr.is_null());

			if (m_protocol == wawo::net::P_WCP) {
				int rt = reuse_addr();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

				rt = reuse_port();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
			}

			WAWO_ASSERT(m_family == addr.family());
			m_laddr = addr;
			return m_fn_bind(m_fd , addr);
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

		SOCKET socket_base::accept(address& addr) {
			return m_fn_accept(m_fd, addr);
		}
	
		int socket_base::connect(address const& addr ) {
			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_raddr.is_null());

			m_sm = SM_ACTIVE;
			WAWO_ASSERT(m_family == addr.family());
			m_raddr = addr;

#ifdef WAWO_IO_MODE_IOCP
			if (m_protocol == P_TCP) {
				//connectex requires the socket to be initially bound
				struct sockaddr_in addr_in;
				::memset(&addr_in, 0, sizeof(addr_in));
				addr_in.sin_family = OS_DEF_family[m_family];
				addr_in.sin_addr.s_addr = INADDR_ANY;
				addr_in.sin_port = 0;
				int bindrt = ::bind( m_fd, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));
				if (bindrt != wawo::OK ) {
					bindrt = wawo::socket_get_last_errno();
					WAWO_DEBUG("bind failed: %d\n", bindrt );
					return bindrt;
				}
				return wawo::E_EINPROGRESS;
			} else {
				WAWO_ASSERT(!"TODO");
			}
#else
			return m_fn_connect(m_fd, addr );
#endif
		}

		int socket_base::turnoff_nodelay() {
			if (!(m_option & OPTION_NODELAY)) {
				return wawo::OK;
			}
			return set_options((m_option & ~OPTION_NODELAY));
		}

		int socket_base::turnon_nodelay() {
			if ((m_option & OPTION_NODELAY)) {
				return wawo::OK;
			}

			return set_options((m_option | OPTION_NODELAY));
		}

		int socket_base::turnon_nonblocking() {
			if ((m_option & OPTION_NON_BLOCKING)) {
				return wawo::OK;
			}
			int op = set_options(m_option | OPTION_NON_BLOCKING);
			return op;
		}

		int socket_base::turnoff_nonblocking() {
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

			WAWO_ERR("[socket_base][%s]setsockopt(SO_SNDBUF) == %d failed, error code: %d", info().to_stdstring().c_str() , size, rt); \
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

			WAWO_ERR("[socket_base][%s]getsockopt(SO_SNDBUF) failed, error code: %d", info().to_stdstring().c_str(), rt);
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

			WAWO_ERR("[socket_base][%s]setsockopt(SO_RCVBUF) == %d failed, error code: %d", info().to_stdstring().c_str(), size, rt);
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

			WAWO_ERR("[socket_base][%s]getsockopt(SO_RCVBUF) failed, error code: %d", info().to_stdstring().c_str() , rt);
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
				WAWO_WARN("[socket_base][%s]socket set options failed, invalid state", info().to_stdstring().c_str() );
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
						WAWO_ERR("[socket_base][%s]socket set socket::OPTION_BROADCAST failed, errno: %d", info().to_stdstring().c_str(), ret );
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
						WAWO_ERR("[socket_base][%s]socket set socket::OPTION_REUSEADDR failed, errno: %d", info().to_stdstring().c_str(), ret );
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
						WAWO_ERR("[socket_base][%s]socket set socket::OPTION_REUSEPORT failed, errno: %d", info().to_stdstring().c_str(), ret);
						return ret;
					}
				}
			}
#endif

			bool _nonblocking_should_set = (((m_option&OPTION_NON_BLOCKING) && ((options&OPTION_NON_BLOCKING) == 0)) ||
				(((m_option&OPTION_NON_BLOCKING) == 0) && ((options&OPTION_NON_BLOCKING))));

			optval = (options & OPTION_NON_BLOCKING) ? 1 : 0;
#ifdef WAWO_ENABLE_WCP
			if (m_protocol == P_WCP) {
				if (_nonblocking_should_set) {
					int mode = socket_api::wcp::fcntl(m_fd, wcp_socket::WCP_F_GETFL, 0);
					if (optval == 1) {
						mode |= WCP_O_NONBLOCK;
					}
					else {
						mode &= WCP_O_NONBLOCK;
					}

					ret = socket_api::wcp::fcntl(m_fd, wcp_socket::WCP_F_SETFL, mode);
				}
			}
			else {
#endif
				if (_nonblocking_should_set) {
					ret = wawo::net::socket_api::helper::turnon_nonblocking(m_fd);
					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_NON_BLOCKING;
						}
						else {
							m_option &= ~OPTION_NON_BLOCKING;
						}
					}
					else {
						WAWO_ERR("[socket_base][%s]socket set socket::OPTION_NON_BLOCKING failed, errno: %d", info().to_stdstring().c_str(), ret);
						return ret;
					}
				}
#ifdef WAWO_ENABLE_WCP
			}
#endif

			if (m_type == T_STREAM) {

				should_set = (((m_option&OPTION_NODELAY) && ((options&OPTION_NODELAY) == 0)) ||
					(((m_option&OPTION_NODELAY) == 0) && ((options&OPTION_NODELAY))));

				if (should_set) {
					
					optval = (options & OPTION_NODELAY) ? 1 : 0;
					ret = wawo::net::socket_api::helper::turnon_nodelay(m_fd);
					if (ret == 0) {
						if (optval == 1) {
							m_option |= OPTION_NODELAY;
						}
						else {
							m_option &= ~OPTION_NODELAY;
						}
					}
					else {
						WAWO_ERR("[socket_base][%s]socket set socket::OPTION_NODELAY failed, errno: %d", info().to_stdstring().c_str(), ret );
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

		wawo::u32_t socket_base::sendto(wawo::byte_t const* const buffer, wawo::u32_t const& len, const wawo::net::address& addr, int& ec_o, int const& flag) {

			//WAWO_ASSERT(!"TOCHECK FOR WCP");

			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);
			ec_o = wawo::OK;

			//if (m_wflag&F_SHUTDOWN_WR) {
			//	ec_o = wawo::E_SOCKET_WR_SHUTDOWN_ALREADY;
			//	return 0;
			//}

			//if (m_state == S_CONNECTED) {
			//	(void)addr;
#ifdef _DEBUG
			//	WAWO_ASSERT( addr == m_raddr );
#endif
			//	return m_fn_send(m_fd, buffer, len, ec_o, flag);
			//}

			return m_fn_sendto(m_fd, buffer, len, addr, ec_o, flag);
		}

		wawo::u32_t socket_base::recvfrom(byte_t* const buffer_o, wawo::u32_t const& size, address& addr_o, int& ec_o) {
			ec_o = wawo::OK;
			//WAWO_ASSERT(!"TOCHECK FOR WCP");
			/*
			if (m_flag&F_SHUTDOWN_RD) {
				ec_o = wawo::E_SOCKET_RD_SHUTDOWN_ALREADY;
				return 0;
			}

			if (m_state == S_CONNECTED) {
#ifdef _DEBUG
				WAWO_ASSERT(!m_raddr.is_null());
#endif
				addr_o = m_raddr;
				return m_fn_recv(m_fd, buffer_o, size, ec_o, 0);
			}
			*/
			return m_fn_recvfrom(m_fd, buffer_o, size, addr_o, ec_o,0);
		}

		wawo::u32_t socket_base::send(byte_t const* const buffer, wawo::u32_t const& len, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);
			return m_fn_send(m_fd, buffer, len, ec_o, flag);
		}

		wawo::u32_t socket_base::recv(byte_t* const buffer_o, wawo::u32_t const& size, int& ec_o, int const& flag) {
			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);
			return m_fn_recv(m_fd, buffer_o, size, ec_o, flag);
		}
}}