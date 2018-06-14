#ifndef _WAWO_NET_SOCKET_BASE_HPP
#define _WAWO_NET_SOCKET_BASE_HPP

#include <wawo/core.hpp>
#include <wawo/thread.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/address.hpp>

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
}}

namespace wawo { namespace net {
	

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
	enum option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 0x01, //only for UDP
		OPTION_REUSEADDR = 0x02,
		OPTION_REUSEPORT = 0x04,
		OPTION_NON_BLOCKING = 0x08,
		OPTION_NODELAY = 0x10, //only for TCP
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

	struct socketinfo {
		int fd;
		s_family f:8;
		s_type t:8;
		s_protocol p:8;

		address laddr;
		address raddr;

		len_cstr to_lencstr() const {
			char _buf[1024] = { 0 };
			int nbytes = snprintf(_buf, 1024, "#%d:L:%s-R:%s:%s", fd, laddr.info().cstr, raddr.info().cstr, protocol_str[p] );
			return wawo::len_cstr(_buf, nbytes);
		}
	};

	class socket_base
	{
	private:
		socket_mode m_sm; //
		s_family	m_family;
		s_type m_type;
		s_protocol m_protocol;

		spin_mutex m_option_mutex;
		int m_option;
		address m_addr;
		address m_bind_addr;

		int m_fd;
		socket_buffer_cfg m_sbc; //socket buffer setting

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

	private:
		void _socket_fn_init();
		int set_options(int const& options);

	protected:
		int open();
		int shutdown(int const& flag);
		int close();
		int bind(address const& addr);
		int listen(int const& backlog);
		int accept(address& addr);
		int connect(address const& addr);

	public:
		explicit socket_base(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt = OPTION_NONE); //by pass a connected socket fd
		explicit socket_base(s_family const& family, s_type const& type, s_protocol const& protocol, option const& opt = OPTION_NONE);
		explicit socket_base(socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& option = OPTION_NONE); //init a empty socket object

		virtual ~socket_base();

		inline socket_buffer_cfg const& buffer_cfg() const { return m_sbc; }
		inline s_family const& sock_family() const { return m_family; };
		inline s_type const& sock_type() const { return m_type; };
		inline s_protocol const& sock_protocol() { return m_protocol; };

		inline bool is_passive() const { return m_sm == SM_PASSIVE; }
		inline bool is_active() const { return m_sm == SM_ACTIVE; }
		inline bool is_tcp() const { return m_protocol == P_TCP; }
		inline bool is_udp() const { return m_protocol == P_UDP; }
		inline bool is_wcp() const { return m_protocol == P_WCP; }
		inline bool is_icmp() const { return m_protocol == P_ICMP; }

		inline bool is_data_socket() const { return m_sm != SM_LISTENER; }
		inline bool is_listener() const { return m_sm == SM_LISTENER; }

		inline int const& fd() const { return m_fd; }
		inline address const& remote_addr() const { return m_addr; }
		address local_addr() const;

		inline socketinfo info() const {
			return socketinfo{ m_fd,m_family,m_type,m_protocol,local_addr(), remote_addr() };
		}

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

		//@hint windows do not provide ways to retrieve idle time and interval ,and probes has been disabled by programming since vista
		int set_keep_alive_vals(keep_alive_vals const& vals);
		int get_keep_alive_vals(keep_alive_vals& vals);

		int get_tos(u8_t& tos) const;
		int set_tos(u8_t const& tos);

		int reuse_addr();
		int reuse_port();



		u32_t send(byte_t const* const buffer, u32_t const& size, int& ec_o, int const& flag = 0);
		u32_t recv(byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0);
		u32_t sendto(byte_t const* const buff, wawo::u32_t const& size, const address& addr, int& ec_o, int const& flag = 0);
		u32_t recvfrom(byte_t* const buff_o, wawo::u32_t const& size, address& addr, int& ec_o);	
	};
}}
#endif