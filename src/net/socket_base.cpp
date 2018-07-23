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

		socket_base::socket_base(SOCKET const& fd, socket_mode const& sm, address const& laddr, address const& raddr, s_family const& family, s_type const& sockt, s_protocol const& proto) :
			m_fd(fd),
			m_sm(sm),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),

			m_laddr(laddr),
			m_raddr(raddr),

			m_cfg(),
			m_child_cfg()
		{
			WAWO_ASSERT(family < F_AF_UNSPEC);
			WAWO_ASSERT(sockt < T_UNKNOWN);
			WAWO_ASSERT(proto < P_UNKNOWN);

			_socket_fn_init();
			WAWO_ASSERT(fd != wawo::E_INVALID_SOCKET );

			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::socket_base(), new connected address: %p", info().to_stdstring().c_str(), this);
		}

		socket_base::socket_base(s_family const& family, s_type const& sockt, s_protocol const& proto) :
			m_fd(wawo::E_INVALID_SOCKET),
			m_sm(SM_NONE),

			m_family(family),
			m_type(sockt),
			m_protocol(proto),

			m_laddr(),
			m_raddr(),

			m_cfg(),
			m_child_cfg()
		{
			WAWO_ASSERT(family < F_AF_UNSPEC);
			WAWO_ASSERT(sockt < T_UNKNOWN);
			WAWO_ASSERT(proto < P_UNKNOWN);

			_socket_fn_init();
			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::socket_base(), dummy socket, address: %p", info().to_stdstring().c_str(), this);
		}

		socket_base::~socket_base() {
			WAWO_TRACE_SOCKET("[socket_base][%s]socket_base::~socket_base(),address: %p", info().to_stdstring().c_str(), this);
		}

		int socket_base::_cfg_reuseaddr(bool onoff) {
			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_OPERATION, m_fd == wawo::E_INVALID_SOCKET);

			bool setornot = ((m_cfg.option&OPTION_REUSEADDR) && (!onoff)) ||
				(((m_cfg.option&OPTION_REUSEADDR) == 0) && (onoff));

			if (!setornot) {
				return wawo::OK;
			}

			int rt = wawo::net::socket_api::helper::set_reuseaddr(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			if (onoff) {
				m_cfg.option |= OPTION_REUSEADDR;
			} else {
				m_cfg.option &= ~OPTION_REUSEADDR;
			}
			return wawo::OK;
		}

		int socket_base::_cfg_reuseport(bool onoff) {
			WAWO_ASSERT(!WAWO_ISWIN);
			(void)onoff;
			
#if WAWO_ISGNU
			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_OPERATION, m_fd == wawo::E_INVALID_SOCKET);

			bool setornot = ((m_cfg.option&OPTION_REUSEPORT) && (!onoff)) ||
				(((m_cfg.option_cfg&OPTION_REUSEPORT) == 0) && (onoff));

			if (!setornot) {
				return wawo::OK;
			}

			int rt = wawo::net::socket_api::helper::set_reuseport(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(rt, rt == wawo::E_SOCKET_ERROR);
			if (onoff) {
				m_cfg.option |= OPTION_REUSEPORT;
			} else {
				m_cfg.option &= ~OPTION_REUSEPORT;
			}
			return wawo::OK;
#else
			return wawo::E_INVALID_OPERATION;
#endif
		}
		int socket_base::_cfg_nonblocking(bool onoff) {
			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_OPERATION, m_fd == wawo::E_INVALID_SOCKET);

			bool setornot = ((m_cfg.option&OPTION_NON_BLOCKING) && (!onoff)) ||
				(((m_cfg.option&OPTION_NON_BLOCKING) == 0) && (onoff));

			if (!setornot) {
				return wawo::OK;
			}

			int rt = wawo::net::socket_api::helper::set_nonblocking(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			if (onoff) {
				m_cfg.option |= OPTION_NON_BLOCKING;
			} else {
				m_cfg.option &= ~OPTION_NON_BLOCKING;
			}
			return wawo::OK;
		}


		int socket_base::_cfg_buffer(socket_buffer_cfg const& cfg) {
			if ( !(m_protocol == P_TCP || m_protocol == P_WCP) ) { return wawo::E_INVALID_OPERATION; }

			int rt;
			u32_t s;
			if (cfg.snd_size == 0) {
				rt = get_snd_buffer_size(s);
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
				m_cfg.buffer.snd_size = s;
			} else {
				rt = set_snd_buffer_size(cfg.snd_size);
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
				m_cfg.buffer.snd_size = cfg.snd_size;
			}

			if (cfg.rcv_size == 0) {
				rt = get_rcv_buffer_size(s);
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
				m_cfg.buffer.rcv_size = s;
			} else {
				rt = set_rcv_buffer_size(cfg.rcv_size);
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
				m_cfg.buffer.rcv_size = cfg.rcv_size;
			}
			return wawo::OK;
		}

		int socket_base::_cfg_nodelay(bool onoff) {
			if (!(m_protocol == P_TCP || m_protocol == P_WCP)) { return wawo::E_INVALID_OPERATION; }

			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_OPERATION, m_fd == wawo::E_INVALID_SOCKET);

			bool setornot = ((m_cfg.option&OPTION_NODELAY) && (!onoff)) ||
				(((m_cfg.option&OPTION_NODELAY) == 0) && (onoff));

			if (!setornot) {
				return wawo::OK;
			}

			int rt = wawo::net::socket_api::helper::set_nodelay(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			if (onoff) {
				m_cfg.option |= OPTION_NODELAY;
			} else {
				m_cfg.option &= ~OPTION_NODELAY;
			}
			return wawo::OK;
		}

		int socket_base::_cfg_keep_alive(bool onoff) {
			if (!(m_protocol == P_TCP || m_protocol == P_WCP)) { return wawo::E_INVALID_OPERATION; }
			//force to false
			int rt = wawo::net::socket_api::helper::set_keep_alive(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			m_cfg.kvals.onoff = onoff;
			return wawo::OK;
		}

		int socket_base::_cfg_keep_alive_vals(keep_alive_vals const& vals) {
			if (!(m_protocol == P_TCP || m_protocol == P_WCP)) { return wawo::E_INVALID_OPERATION; }
			WAWO_ASSERT(!is_listener());
			int rt = _cfg_keep_alive(vals.onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			WAWO_RETURN_V_IF_MATCH(wawo::OK, vals.onoff == false);

#if WAWO_ISWIN
			DWORD dwBytesRet;
			struct tcp_keepalive alive;
			::memset(&alive, 0, sizeof(alive));

			alive.onoff = 1;
			if (vals.idle == 0) {
				alive.keepalivetime = WAWO_DEFAULT_KEEPALIVE_IDLETIME;
			} else {
				alive.keepalivetime = vals.idle;
			}

			if (vals.interval == 0) {
				alive.keepaliveinterval = WAWO_DEFAULT_KEEPALIVE_INTERVAL;
			} else {
				alive.keepaliveinterval = vals.idle;
			}

			rt = ::WSAIoctl(m_fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
#elif WAWO_ISGNU
			rt = _cfg_set_keep_alive(true);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

			if (vals.idle != 0) {
				i32_t idle = (vals.idle / 1000);
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			}
			if (vals.interval != 0) {
				i32_t interval = (vals.interval / 1000);
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			}
			if (vals.probes != 0) {
				rt = m_fn_setsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &vals.probes, sizeof(vals.probes));
				WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			}
#else
#error
#endif
			m_cfg.kvals = vals;
			return wawo::OK;
		}

		int socket_base::_cfg_broadcast(bool onoff) {
			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_OPERATION, m_fd == wawo::E_INVALID_SOCKET);
			WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_INVALID_OPERATION, m_protocol == P_UDP);

			bool setornot = ((m_cfg.option&OPTION_BROADCAST) && (!onoff)) ||
				(((m_cfg.option&OPTION_BROADCAST) == 0) && (onoff));

			if (!setornot) {
				return wawo::OK;
			}
			int rt = wawo::net::socket_api::helper::set_broadcast(m_fd, onoff);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
			if (onoff) {
				m_cfg.option |= OPTION_BROADCAST;
			} else {
				m_cfg.option &= ~OPTION_BROADCAST;
			}
			return wawo::OK;
		}

		int socket_base::_cfgs_setup_common(socket_cfg const& cfg) {
			//int rt = _cfg_nonblocking((cfg.option&OPTION_NON_BLOCKING) != 0 );
			//WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

			//force nonblocking
			int rt = _cfg_nonblocking(true);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

			//child will inherite parent's buffer setting
			rt = _cfg_buffer(cfg.buffer);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

			rt = _cfg_reuseaddr((cfg.option&OPTION_REUSEADDR) !=0);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

#if WAWO_ISGNU
			rt = _cfg_reuseport((cfg.option&OPTION_REUSEPORT)!=0);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);
#endif
			return wawo::OK;
		}

		int socket_base::_cfg_setup_udp(socket_cfg const& cfg) {
			return _cfg_broadcast((cfg.option&OPTION_BROADCAST) != 0);
		}

		//wcp share same cfg with tcp
		int socket_base::_cfg_setup_tcp(socket_cfg const& cfg) {
			int rt = _cfg_nodelay((cfg.option&OPTION_NODELAY) != 0);
			WAWO_RETURN_V_IF_MATCH(wawo::E_SOCKET_ERROR, rt == wawo::E_SOCKET_ERROR);

			return _cfg_keep_alive_vals(cfg.kvals);
		}

		int socket_base::open(socket_cfg const& cfg ) {
			WAWO_ASSERT(cfg.buffer.rcv_size <= SOCK_RCV_MAX_SIZE);
			WAWO_ASSERT(cfg.buffer.snd_size <= SOCK_SND_MAX_SIZE);

			WAWO_ASSERT(m_fd == wawo::E_INVALID_SOCKET);
			m_fd = m_fn_socket(m_family, m_type, m_protocol);
			WAWO_RETURN_V_IF_MATCH(wawo::E_INVALID_SOCKET, m_fd == wawo::E_INVALID_SOCKET);

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

		int socket_base::bind( address const& addr) {
			WAWO_ASSERT(m_sm == SM_NONE);
			WAWO_ASSERT(m_laddr.is_null());

			if (m_protocol == wawo::net::P_WCP) {
				int rt = reuse_addr();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
#if !WAWO_ISWIN
				rt = reuse_port();
				WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
#endif
			}

			WAWO_ASSERT(m_family == addr.family());
			m_laddr = addr;
			return m_fn_bind(m_fd , addr);
		}

		int socket_base::listen(socket_cfg const& child_cfg, int const& backlog) {
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
			m_child_cfg = child_cfg;
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


		int socket_base::set_snd_buffer_size(u32_t const& size) {
			WAWO_ASSERT(size >= SOCK_SND_MIN_SIZE && size <= SOCK_SND_MAX_SIZE);
			WAWO_ASSERT(m_fd > 0);

			int rt = setsockopt(SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size));
			if (wawo::OK != rt) {
				WAWO_ERR("[socket_base][%s]setsockopt(SO_SNDBUF) == %d failed, error code: %d", info().to_stdstring().c_str(), size, rt); \
				return rt;
			}

#ifdef _DEBUG
			u32_t news;
			rt = get_snd_buffer_size(news);
			WAWO_ASSERT(rt == wawo::OK);
			WAWO_TRACE_SOCKET("[socket_base][%s]new snd buffer size: %u", info().to_stdstring().c_str(), news);
#endif
			m_cfg.buffer.snd_size = size;
			return wawo::OK;
		}

		int socket_base::get_snd_buffer_size(u32_t& size) const {
			WAWO_ASSERT(m_fd > 0);

			socklen_t opt_length = sizeof(u32_t);
			int rt = m_fn_getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&size, &opt_length);
			if (rt != wawo::OK) {
				WAWO_ERR("[socket_base][%s]getsockopt(SO_SNDBUF) failed, error code: %d", info().to_stdstring().c_str(), rt);
				return rt;
			}
			return wawo::OK;
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
			WAWO_ASSERT(m_fd != wawo::E_INVALID_SOCKET );

			int rt = setsockopt(SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size));
			if (wawo::OK != rt) {
				WAWO_ERR("[socket_base][%s]setsockopt(SO_RCVBUF) == %d failed, error code: %d", info().to_stdstring().c_str(), size, rt);
				return rt;
			}
#ifdef _DEBUG
			u32_t news;
			rt = get_rcv_buffer_size(news);
			WAWO_ASSERT(rt == wawo::OK);
			WAWO_TRACE_SOCKET("[socket_base][%s]new rcv buffer size: %u", info().to_stdstring().c_str(), news);
#endif
			m_cfg.buffer.rcv_size = size;
			return wawo::OK;
		}

		int socket_base::get_rcv_buffer_size(u32_t& size) const {
			WAWO_ASSERT(m_fd > 0);

			socklen_t opt_length = sizeof(u32_t);
			int rt = m_fn_getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&size, &opt_length);
			if (rt != wawo::OK ) {
				WAWO_ERR("[socket_base][%s]getsockopt(SO_RCVBUF) failed, error code: %d", info().to_stdstring().c_str(), rt);
				return rt;
			}
			return wawo::OK;
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
			soLinger.l_linger = (linger_t&0xFFFF);
			return setsockopt(SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
		}

		/*
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
		*/

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

		wawo::u32_t socket_base::sendto(wawo::byte_t const* const buffer, wawo::u32_t const& len, const wawo::net::address& addr, int& ec_o, int const& flag) {

			//WAWO_ASSERT(!"TOCHECK FOR WCP");

			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(len > 0);
			ec_o = wawo::OK;

			//if (m_wflag&F_WRITE_SHUTDOWN) {
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
			if (m_flag&F_READ_SHUTDOWN) {
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