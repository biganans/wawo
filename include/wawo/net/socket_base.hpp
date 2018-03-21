#ifndef _WAWO_NET_SOCKET_BASE_HPP
#define _WAWO_NET_SOCKET_BASE_HPP

#include <wawo/core.hpp>
#include <wawo/thread/thread.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/address.hpp>
#include <wawo/net/tlp_abstract.hpp>

#include <wawo/net/socket_observer.hpp>

//#define WAWO_ENABLE_TRACE_SOCKET
#ifdef WAWO_ENABLE_TRACE_SOCKET
	#define WAWO_TRACE_SOCKET WAWO_INFO
#else
	#define WAWO_TRACE_SOCKET(...)
#endif

#ifdef WAWO_ENABLE_TRACE_SOCKET_INOUT
	#define WAWO_TRACE_SOCKET_INOUT WAWO_INFO
#else
	#define WAWO_TRACE_SOCKET_INOUT(...)
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#ifndef EAGAIN //EAGAIN IS NOT ALWAYS THE SAME AS EWOULDBLOCK
#define EAGAIN EWOULDBLOCK
#endif

#ifndef EISCONN
#define EISCONN WSAEISCONN
#endif

#ifndef ENOTINITIALISED
#define ENOTINITIALISED WSANOTINITIALISED
#endif

#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==EAGAIN)||(_errno==EWOULDBLOCK)||(_errno==WSAEWOULDBLOCK))
#define IS_ERRNO_EQUAL_CONNECTING(_errno) ((_errno==EINPROGRESS)||(_errno==WSAEWOULDBLOCK))

#define WAWO_MAX_ACCEPTS_ONE_TIME 128

#ifndef IPTOS_TOS_MASK
#define IPTOS_TOS_MASK		0x1E
#endif

#ifndef IPTOS_TOS
#define IPTOS_TOS(tos)		((tos)&IPTOS_TOS_MASK)
#endif

#ifndef IPTOS_LOWDELAY
#define	IPTOS_LOWDELAY		0x10
#endif

#ifndef IPTOS_THROUGHPUT
#define	IPTOS_THROUGHPUT	0x08
#endif

#ifndef IPTOS_RELIABILITY
#define	IPTOS_RELIABILITY	0x04
#endif

#ifndef IPTOS_MINCOST
#define	IPTOS_MINCOST		0x02
#endif


#define __FLUSH_DEFAULT_BLOCK_TIME__		(5*1000)	//5 ms
#define __FLUSH_BLOCK_TIME_INFINITE__		(-1)		//
#define __FLUSH_SLEEP_TIME_MAX__			(200)		//0.2 ms
#define __WAIT_FACTOR__						(50)		//50 microseconds

namespace wawo { namespace net {

	#ifdef WAWO_PLATFORM_GNU
		typedef socklen_t socklen_t;
	#elif defined(WAWO_PLATFORM_WIN)
		typedef int socklen_t;
	#else
		#error
	#endif

	typedef int (*fn_socket)(int const& family, int const& socket_type, int const& protocol);
	typedef int (*fn_connect)(int const& fd, const struct sockaddr* addr, socklen_t const& length);
	typedef int (*fn_bind)(int const& fd, const struct sockaddr* addr, socklen_t const& length);
	typedef int (*fn_shutdown)(int const& fd, int const& flag);
	typedef int (*fn_close)(int const& fd);
	typedef int (*fn_listen)(int const& fd, int const& backlog);
	typedef int (*fn_accept)(int const& fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef int(*fn_getsockopt)(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len);
	typedef int(*fn_setsockopt)(int const& fd, int const& level, int const& option_name, void const* optval, socklen_t const& option_len);
	typedef int(*fn_getsockname)(int const& fd, struct sockaddr* addr, socklen_t* addrlen);
	typedef u32_t (*fn_send)(int const& fd, byte_t const* const buffer, u32_t const& length, int& ec_o, int const& flag);
	typedef u32_t (*fn_recv)(int const&fd, byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag);
	typedef u32_t (*fn_sendto)(int const& fd, wawo::byte_t const* const buff, wawo::u32_t const& length, const wawo::net::address& addr, int& ec_o, int const& flag);
	typedef u32_t (*fn_recvfrom)(int const& fd, byte_t* const buff_o, wawo::u32_t const& size, address& addr_o, int& ec_o, int const& flag );

	class socket_base;
	struct async_cookie :
		public ref_base
	{
		WWRP<socket_base> so;
		WWRP<ref_base> user_cookie;
		fn_io_event success;
		fn_io_event_error error;
	};

	void async_connected(WWRP<ref_base> const& cookie_);
	void async_connect_error(int const& code, WWRP<ref_base> const& cookie_);

	void async_handshake(WWRP<ref_base> const& cookie);
	void async_handshake_error(int const& code, WWRP<ref_base> const& cookie);

	void async_accept(WWRP<ref_base> const& so_);
	void async_read(WWRP<ref_base> const& so_);
	void async_write(WWRP<ref_base> const& so_);
	void async_error(int const& code, WWRP<ref_base> const& so_);
}}

namespace wawo { namespace net {
	using namespace wawo::thread;

	enum socket_buffer_cfg_type {
		BT_SUPER_LARGE,
		BT_LARGE,
		BT_MEDIUM_UPPER,
		BT_MEDIUM,
		BT_TINY,
		BT_SUPER_TINY,
		BT_MAX
	};
	#define BT_DEFAULT BT_MEDIUM

	struct socket_buffer_cfg {
		u32_t rcv_size;
		u32_t snd_size;
	};

	const static socket_buffer_cfg socket_buffer_cfgs[BT_MAX] =
	{
		{ ((1024 * 1024 * 2) - WAWO_MALLOC_H_RESERVE),((1024 * 1024 * 2) - WAWO_MALLOC_H_RESERVE) }, //2M //super large
		{ (1024 * 768 - WAWO_MALLOC_H_RESERVE),		(1024 * 768 - WAWO_MALLOC_H_RESERVE) }, //768K large
		{ (1024 * 256 - WAWO_MALLOC_H_RESERVE),		(1024 * 256 - WAWO_MALLOC_H_RESERVE) }, //256k medium upper
		{ 1024 * 96,	1024 * 96 }, //96k medium
		{ 1024 * 32,	1024 * 32 }, //32k tiny
		{ 1024 * 8,		1024 * 8 }, //8k, super tiny
	};

	enum socket_buffer_range {
		SOCK_SND_MAX_SIZE = 1024 * 1024 * 2,
		SOCK_SND_MIN_SIZE = 1024 * 4,
		SOCK_RCV_MAX_SIZE = 1024 * 1024 * 2,
		SOCK_RCV_MIN_SIZE = 1024 * 4
	};

	enum socket_mode {
		SM_NONE,
		SM_ACTIVE,
		SM_PASSIVE,
		SM_LISTENER
	};
	enum Option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 0x01, //only for UDP
		OPTION_REUSEADDR = 0x02,
		OPTION_REUSEPORT = 0x04,
		OPTION_NON_BLOCKING = 0x08,
		OPTION_NODELAY = 0x10, //only for TCP
	};

	enum socket_flag {
		SHUTDOWN_NONE = 0,
		SHUTDOWN_RD = 1,
		SHUTDOWN_WR = 1 << 1,
		SHUTDOWN_RDWR = (SHUTDOWN_RD | SHUTDOWN_WR),

		WATCH_READ = 1 << 2,
		WATCH_WRITE = 1 << 3,

		WATCH_OPTION_INFINITE = 1 << 4,
		WATCH_OPTION_POST_READ_EVENT_AFTER_WATCH = 1 << 5
	};

	enum socket_state {
		S_CLOSED,
		S_OPENED,
		S_BINDED,
		S_LISTEN,
		S_CONNECTING,// for async connect
		S_CONNECTED,
	};

	enum LockType {
		L_SOCKET = 0,
		L_READ,
		L_WRITE,
		L_MAX
	};

	struct keep_alive_vals {
		u8_t	onoff;
		i32_t	idle; //in milliseconds
		i32_t	interval; //in milliseconds
		i32_t	probes;

		keep_alive_vals() :
			onoff(0),
			idle(0),
			interval(0),
			probes(0)
		{}

		keep_alive_vals(u8_t const& onoff_, i32_t const& idle_, i32_t const& interval_, i32_t const& probes_):
			onoff(onoff_),
			idle(idle_),
			interval(interval_),
			probes(probes_)
		{
		}
	};

	const static keep_alive_vals default_keep_alive_vals = {1,(60*1000),(30*1000),10};

	class socket_base :
		public wawo::ref_base
	{

	protected:
		socket_mode m_sm; //
		family	m_family;
		sock_type m_type;
		protocol_type m_protocol;

		int m_option;
		address m_addr;
		address m_bind_addr;

		int m_fd;
		spin_mutex m_mutexes[L_MAX];
		socket_buffer_cfg m_sbc; //socket buffer setting

		WWRP<tlp_abstract>	m_tlp;
		WWRP<ref_base>	m_ctx;

		u8_t	m_state;
//		u8_t	m_flag;
		u8_t	m_rflag;
		u8_t	m_wflag;

		fn_socket m_fn_socket;
		fn_connect m_fn_connect;
		fn_bind m_fn_bind;
		fn_shutdown m_fn_shutdown;
		fn_close m_fn_close;
		fn_listen m_fn_listen;
		fn_accept m_fn_accept;
		fn_send m_fn_send;
		fn_recv m_fn_recv;
		fn_sendto m_fn_sendto;
		fn_recvfrom m_fn_recvfrom;

		fn_getsockopt m_fn_getsockopt;
		fn_setsockopt m_fn_setsockopt;

		fn_getsockname m_fn_getsockname;
	protected:
		void _socket_fn_init();
		int _close(int const& ec = wawo::OK);
		int _shutdown(u8_t const& flag, u8_t& sflag);
		int _connect(wawo::net::address const& addr);
		int _set_options(int const& options);
	public:
		explicit socket_base(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc, family const& family, sock_type const& sockt, protocol_type const& proto, Option const& option = OPTION_NONE); //by pass a connected socket fd
		explicit socket_base(family const& family, sock_type const& type, protocol_type const& protocol, Option const& option = OPTION_NONE);
		explicit socket_base(socket_buffer_cfg const& sbc, family const& family, sock_type const& sockt, protocol_type const& proto, Option const& option = OPTION_NONE); //init a empty socket object

		virtual ~socket_base();

		socket_buffer_cfg get_buffer_cfg() const { return m_sbc; }

		inline bool is_passive() const { return m_sm == SM_PASSIVE; }
		inline bool is_active() const { return m_sm == SM_ACTIVE; }
		inline bool is_tcp() const { return m_protocol == P_TCP; }
		inline bool is_udp() const { return m_protocol == P_UDP; }
		inline bool is_wcp() const { return m_protocol == P_WCP; }
		inline bool is_icmp() const { return m_protocol == P_ICMP; }

		inline bool is_data_socket() const { return m_sm != SM_LISTENER; }
		inline bool is_listener() const { return m_sm == SM_LISTENER; }

		inline int const& get_fd() const { return m_fd; }
		address const& get_remote_addr() const { return m_addr; }
		address get_local_addr() const;

		len_cstr get_addr_info() {
			if (is_listener()) return m_bind_addr.address_info();
			return get_remote_addr().address_info();
		}

		int open();
		int shutdown(u8_t const& flag, int const& ec = wawo::OK);
		int close(int const& ec = wawo::OK);

		inline bool is_connected() const { return m_state == S_CONNECTED; }
		inline bool is_connecting() const { return (m_state == S_CONNECTING); }
		inline bool is_closed() const { return (m_state == S_CLOSED); }

		inline bool is_read_shutdowned() const { return (m_rflag&SHUTDOWN_RD) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&SHUTDOWN_WR) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag|m_wflag)&SHUTDOWN_RDWR) == SHUTDOWN_RDWR); }

		int bind(address const& addr);

		//int listen(int const& backlog, fn_io_event const& cb_accepted = NULL, WWRP<ref_base> const& cookie = NULL );

		//return socketFd if success, otherwise return -1 if an error is set
		//u32_t accept(WWRP<socket_base> sockets[], u32_t const& size, int& ec_o);
		int connect(address const& addr);
		int async_connect(address const& addr, WWRP<ref_base> const& cookie, fn_io_event const& success, fn_io_event_error const& error);

		u32_t send(byte_t const* const buffer, u32_t const& size, int& ec_o, int const& flag = 0);
		u32_t recv(byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0);

		u32_t sendto(byte_t const* const buff, wawo::u32_t const& size, const address& addr, int& ec_o, int const& flag = 0);
		u32_t recvfrom(byte_t* const buff_o, wawo::u32_t const& size, address& addr, int& ec_o);

		inline int getsockopt(int const& level, int const& option_name, void* value, socklen_t* option_len) {
			return m_fn_getsockopt(m_fd, level, option_name, value, option_len);
		}

		inline int setsockopt(int const& level, int const& option_name, void const* value, socklen_t const& option_len) {
			return m_fn_setsockopt(m_fd, level, option_name, value, option_len);
		}

		inline bool is_nodelay() const { return ((m_option&OPTION_NODELAY) != 0); }

		int turnoff_nodelay();
		int turnon_nodelay();

		int turnon_nonblocking();
		int turnoff_nonblocking();
		inline bool is_nonblocking() const { return ((m_option&OPTION_NON_BLOCKING) != 0); }

		//must be called between open and connect|listen
		int set_snd_buffer_size(u32_t const& size);
		int get_snd_buffer_size(u32_t& size) const;
		int get_left_snd_queue(u32_t& size) const;


		//must be called between open and connect|listen
		int set_rcv_buffer_size(u32_t const& size);
		int get_rcv_buffer_size(u32_t& size) const;
		int get_left_rcv_queue(u32_t& size) const;

		int get_linger(bool& on_off, int& linger_t) const;
		int set_linger(bool const& on_off, int const& linger_t = 30 /* in seconds */);

		int turnon_keep_alive();
		int turnoff_keep_alive();

		/*	please note that
		windows do not provide ways to retrieve idle time and interval ,and probes has been disabled by programming since vista
		*/
		int set_keep_alive_vals(keep_alive_vals const& vals);
		int get_keep_alive_vals(keep_alive_vals& vals);

		int get_tos(u8_t& tos) const;
		int set_tos(u8_t const& tos);

		int reuse_addr();
		int reuse_port();

		inline void handle_async_connected() {
			lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
			WAWO_ASSERT(m_fd > 0);
			WAWO_ASSERT(m_sm == SM_ACTIVE);
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			m_state = S_CONNECTED;
		}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			lock_guard<spin_mutex> _lg(*(const_cast<spin_mutex*>(&m_mutexes[L_SOCKET])));
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
			m_ctx = ctx;
		}

		inline WWRP<tlp_abstract> const& get_tlp() const {
			lock_guard<spin_mutex> _lg(*(const_cast<spin_mutex*>(&m_mutexes[L_SOCKET])));
			return m_tlp;
		}

		inline void set_tlp(WWRP<tlp_abstract> const& tlp) {
			lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
			WAWO_ASSERT(m_tlp == NULL);
			WAWO_ASSERT(tlp != NULL);
			m_tlp = tlp;
		}

		int tlp_handshake(WWRP<ref_base> const& cookie = NULL, fn_io_event const& success = NULL, fn_io_event_error const& error = NULL) {
			(void)cookie;
			(void)success;
			(void)error;
			WAWO_THROW("logic error, socket_base only for internal");
			return wawo::E_TLP_HANDSHAKE_DONE;
		}

		void handle_async_handshake(int& ec_o) { (void)ec_o; WAWO_THROW("logic error, socket_base only for internal"); }
		void handle_async_read(int& ec_o) { (void)ec_o; WAWO_THROW("logic error, socket_base only for internal"); }
		void handle_async_write(int& ec_o) { (void)ec_o; WAWO_THROW("logic error, socket_base only for internal"); }

	protected:
		inline void _end_async_read() {
			if ((m_rflag&WATCH_READ) && is_nonblocking()) {
				m_rflag &= ~(WATCH_READ|WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][#%d:%s][end_async_read]unwatch IOE_READ", m_fd, get_addr_info().cstr);
				unwatch(is_wcp(), IOE_READ, m_fd);
			}
#ifdef _DEBUG
			else {
				TRACE_IOE("[socket][#%d:%s][end_async_read]unwatch IOE_READ, no op", m_fd, get_addr_info().cstr);
			}
#endif
		}

		inline void _end_async_write() {
			if ((m_wflag&WATCH_WRITE) && is_nonblocking()) {
				m_wflag &= ~(WATCH_WRITE| WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][#%d:%s][end_async_write]unwatch IOE_WRITE", m_fd, get_addr_info().cstr);
				unwatch(is_wcp(), IOE_WRITE, m_fd);
			}
#ifdef _DEBUG
			else {
				TRACE_IOE("[socket][#%d:%s][end_async_write]unwatch IOE_WRITE, no op", m_fd, get_addr_info().cstr);
			}
#endif
		}

	public:

		inline void begin_async_connect(WWRP<ref_base> const& cookie , fn_io_event const& fn , fn_io_event_error const& err ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);

			WAWO_ASSERT(!(m_wflag&SHUTDOWN_WR));
			WAWO_ASSERT(!(m_wflag&WATCH_WRITE));
			m_wflag |= WATCH_WRITE;
			TRACE_IOE("[socket][#%d:%s][begin_async_connect]watch IOE_WRITE", m_fd, get_addr_info().cstr);

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->so = WWRP<socket_base>(this);
			_cookie->user_cookie = cookie;
			_cookie->success = fn;
			_cookie->error = err;

			watch(is_wcp(), IOE_WRITE, m_fd, _cookie, wawo::net::async_connected, wawo::net::async_connect_error );
		}

		inline void end_async_connect() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_WRITE]);
			_end_async_write();
		}

		inline void begin_async_handshake(WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err) {
			WAWO_ASSERT(is_nonblocking());

			WAWO_ASSERT(cookie != NULL);
			WAWO_ASSERT(fn != NULL);
			WAWO_ASSERT(err != NULL);

			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			if (m_rflag&SHUTDOWN_RD) {
				TRACE_IOE("[socket][#%d:%s][begin_async_handshake]cancel for rd shutdowned already", m_fd, get_addr_info().cstr);
				auto lambda = [err, cookie]() -> void {
					err(wawo::E_INVALID_STATE, cookie);
				};
				WAWO_SCHEDULER->schedule(lambda);
				return;
			}

			WAWO_ASSERT(!(m_rflag&WATCH_READ));
			m_rflag |= WATCH_READ;
			TRACE_IOE("[socket][#%d:%s][begin_async_handshake]watch IOE_READ", m_fd, get_addr_info().cstr);

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->so = WWRP<socket_base>(this);
			_cookie->user_cookie = cookie;
			_cookie->success = fn;
			_cookie->error = err;

			u8_t flag = IOE_READ | IOE_INFINITE_WATCH_READ;
			watch(is_wcp(), flag, m_fd, _cookie, wawo::net::async_handshake, wawo::net::async_handshake_error);
		}

		inline void end_async_handshake() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			_end_async_read();
		}

		inline void _begin_async_read( u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_read, fn_io_event_error const& err = wawo::net::async_error) {
			WAWO_ASSERT(is_nonblocking());

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->user_cookie = cookie;
			_cookie->so = WWRP<socket_base>(this);

			if (m_rflag&SHUTDOWN_RD) {
				TRACE_IOE("[socket][#%d:%s][begin_async_read]cancel for rd shutdowned already", m_fd, get_addr_info().cstr);
				auto lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};
				WAWO_SCHEDULER->schedule(lambda);
				return;
			}

			if (m_rflag&WATCH_READ) {
				TRACE_IOE("[socket][#%d:%s][begin_async_read]ignore for watch already", m_fd, get_addr_info().cstr);
				return;
			}

			WAWO_ASSERT(!(m_rflag&WATCH_READ));

			m_rflag |= (WATCH_READ| async_flag);
			TRACE_IOE("[socket][#%d:%s][begin_async_read]watch IOE_READ", m_fd, get_addr_info().cstr);

			u8_t flag = IOE_READ;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_READ;
			}
			watch(is_wcp(), flag, m_fd, _cookie, fn, err);

			if (async_flag&WATCH_OPTION_POST_READ_EVENT_AFTER_WATCH) {
				wawo::task::fn_lambda _lambda_ = [fn,_cookie]() -> void {
					fn(_cookie);
				};
				WAWO_SCHEDULER->schedule(_lambda_);
			}
		}

		inline void begin_async_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_read, fn_io_event_error const& err = wawo::net::async_error) {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			_begin_async_read(async_flag, cookie, fn ,err);
		}

		inline void end_async_read() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			_end_async_read();
		}

		inline void _begin_async_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_write, fn_io_event_error const& err = wawo::net::async_error ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->user_cookie = cookie;
			_cookie->so = WWRP<socket_base>(this);

			if (m_wflag&SHUTDOWN_WR) {
				TRACE_IOE("[socket][#%d:%s][begin_async_write]cancel for wr shutdowned already", m_fd, get_addr_info().cstr);
				auto lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};
				WAWO_SCHEDULER->schedule(lambda);
				return;
			}

			if (m_wflag&WATCH_WRITE) {
				TRACE_IOE("[socket][#%d:%s][begin_async_write]ignore for write watch already", m_fd, get_addr_info().cstr);
				return;
			}

			m_wflag |= (WATCH_WRITE|async_flag);
			TRACE_IOE("[socket][#%d:%s][begin_async_write]watch IOE_WRITE", m_fd, get_addr_info().cstr);

			u8_t flag = IOE_WRITE;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_WRITE;
			}
			watch(is_wcp(), flag, m_fd, _cookie, fn, err);
		}

		inline void begin_async_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_write, fn_io_event_error const& err = wawo::net::async_error) {
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			_begin_async_write(async_flag, cookie, fn, err);
		}

		inline void end_async_write() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_WRITE]);
			_end_async_write();
		}
	};
}}
#endif