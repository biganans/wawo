#ifndef _WAWO_NET_SOCKET_BASE_HPP
#define _WAWO_NET_SOCKET_BASE_HPP

#include <wawo/core.hpp>
#include <wawo/thread.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/address.hpp>
#include <wawo/net/socket_api.hpp>

#define WAWO_ENABLE_TRACE_SOCKET
#ifdef WAWO_ENABLE_TRACE_SOCKET
	#define WAWO_TRACE_SOCKET WAWO_INFO
#else
	#define WAWO_TRACE_SOCKET(...)
#endif

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

		std::string to_stdstring() const {
			char _buf[1024] = { 0 };
			int nbytes = snprintf(_buf, 1024, "#%d:%s:L:%s-R:%s", fd, protocol_str[p], laddr.info().cstr, raddr.info().cstr );
			return std::string(_buf, nbytes);
		}
	};

	class socket_base
	{
	protected:
		int m_fd;
		socket_mode m_sm; //
		s_family m_family;
		s_type m_type;
		s_protocol m_protocol;

		int m_option;
		address m_laddr;
		address m_raddr;

		socket_buffer_cfg m_sbc; //socket buffer setting

		socket_api::fn_socket m_fn_socket;
		socket_api::fn_connect m_fn_connect;
		socket_api::fn_bind m_fn_bind;
		socket_api::fn_shutdown m_fn_shutdown;
		socket_api::fn_close m_fn_close;
		socket_api::fn_listen m_fn_listen;
		socket_api::fn_accept m_fn_accept;
		socket_api::fn_send m_fn_send;
		socket_api::fn_recv m_fn_recv;
		socket_api::fn_sendto m_fn_sendto;
		socket_api::fn_recvfrom m_fn_recvfrom;

		socket_api::fn_getsockopt m_fn_getsockopt;
		socket_api::fn_setsockopt m_fn_setsockopt;

		socket_api::fn_getsockname m_fn_getsockname;

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
		explicit socket_base(int const& fd, address const& laddr, address const& raddr, socket_mode const& sm, socket_buffer_cfg const& sbc, s_family const& family, s_type const& sockt, s_protocol const& proto, option const& opt = OPTION_NONE); //by pass a connected socket fd
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

		inline bool is_listener() const { return m_sm == SM_LISTENER; }

		inline int const& fd() const { return m_fd; }
		inline address const& remote_addr() const { return m_raddr; }
		inline address const& local_addr() const { return m_laddr; }
		
		int load_local_addr() {
			if (!m_laddr.is_null()) return wawo::OK;
			struct sockaddr_in addr_in;
			socklen_t addr_in_length = sizeof(addr_in);
			int rt = m_fn_getsockname(m_fd, (struct sockaddr*) &addr_in, &addr_in_length);
			if (rt == wawo::OK) {
				WAWO_ASSERT(addr_in.sin_family == AF_INET);
				m_laddr = address(addr_in);
				return wawo::OK;
			}
			return wawo::socket_get_last_errno();
		}

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