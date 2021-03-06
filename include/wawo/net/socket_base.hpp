#ifndef _WAWO_NET_SOCKET_BASE_HPP
#define _WAWO_NET_SOCKET_BASE_HPP

#include <wawo/core.hpp>
#include <wawo/thread.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/address.hpp>
#include <wawo/net/socket_api.hpp>

//#define WAWO_ENABLE_TRACE_SOCKET
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

	const static socket_buffer_cfg default_socket_buffer_cfg = socket_buffer_cfgs[BT_DEFAULT];

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
	enum socket_option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 0x01, //only for UDP
		OPTION_REUSEADDR = 0x02,
		OPTION_REUSEPORT = 0x04,
		OPTION_NON_BLOCKING = 0x08,
		OPTION_NODELAY = 0x10, //only for TCP
	};
	const static int default_socket_option = OPTION_NONE;

	struct keep_alive_vals {
		bool	onoff;
		i32_t	idle; //in milliseconds
		i32_t	interval; //in milliseconds
		i32_t	probes;

		keep_alive_vals() :
			onoff(true),
			idle(0),
			interval(0),
			probes(0)
		{}

		keep_alive_vals(bool const& onoff_, i32_t const& idle_, i32_t const& interval_, i32_t const& probes_):
			onoff(onoff_),
			idle(idle_),
			interval(interval_),
			probes(probes_)
		{
		}
	};

	const static keep_alive_vals default_keep_alive_vals(true,(60*1000),(30*1000),10);

	struct socket_cfg {
		int option;
		socket_buffer_cfg buffer;
		keep_alive_vals kvals;
		socket_cfg() :
			option(OPTION_NONE),
			buffer({0,0}),
			kvals({false,0,0,0})
		{}
		socket_cfg(int option_):
			option(option_),
			buffer({ 0,0 }),
			kvals({ false,0,0,0 })
		{
		}

		socket_cfg(int option_ ,socket_buffer_cfg const& bcfg_ ) :
			option(option_),
			buffer(bcfg_),
			kvals({ false,0,0,0 })
		{
		}

		socket_cfg(int option_, socket_buffer_cfg const& bcfg_, keep_alive_vals const& kvals_):
			option(option_),
			buffer(bcfg_),
			kvals(kvals_)
		{}
	};

	const static socket_cfg default_socket_cfg(OPTION_NON_BLOCKING, { 0,0 }, { true,0,0,0 });

	struct socketinfo {
		SOCKET fd;
		s_family f:8;
		s_type t:8;
		s_protocol p:8;

		address laddr;
		address raddr;

		std::string to_stdstring() const {
			char _buf[1024] = { 0 };
#if WAWO_IS_ADDRESS_MODE_X64
			int nbytes = snprintf(_buf, 1024, "#%llu:%s:L:%s-R:%s", fd, protocol_str[p], laddr.info().c_str(), raddr.info().c_str() );
#else
			int nbytes = snprintf(_buf, 1024, "#%u:%s:L:%s-R:%s", fd, protocol_str[p], laddr.info().c_str(), raddr.info().c_str());
#endif
			return std::string(_buf, nbytes);
		}
	};

	class socket_base
	{
	protected:
		SOCKET m_fd;
		socket_mode m_sm; //

		s_family m_family;
		s_type m_type;
		s_protocol m_protocol;

		address m_laddr;
		address m_raddr;
		socket_cfg m_cfg;
		socket_cfg m_child_cfg; //for listen fd

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
	protected:
		int _cfg_reuseaddr(bool onoff);
		int _cfg_reuseport(bool onoff);
		int _cfg_nonblocking(bool onoff);

		int _cfg_buffer( socket_buffer_cfg const& bcfg );
		int _cfg_nodelay(bool onoff);
		int _cfg_keep_alive(bool onoff);
		int _cfg_keep_alive_vals(keep_alive_vals const& vals);

		int _cfg_broadcast(bool onoff);
		int _cfgs_setup_common(socket_cfg const& cfg);
		int _cfg_setup_udp(socket_cfg const& cfg);
		int _cfg_setup_tcp(socket_cfg const& cfg);
	protected:
		int init(socket_cfg const& cfg) {
			int rt = _cfgs_setup_common(cfg);
			WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
			return m_protocol == P_UDP ? _cfg_setup_udp(cfg) : _cfg_setup_tcp(cfg);
		}
		int open();
		int shutdown(int const& flag);
		int close();
		int bind(address const& addr);
		int listen(socket_cfg const& child_cfg, int const& backlog);
		SOCKET accept(address& addr);
		int connect(address const& addr);

	public:
		explicit socket_base(SOCKET const& fd, socket_mode const& sm, address const& laddr, address const& raddr, s_family const& family, s_type const& sockt, s_protocol const& proto ); //by pass a connected socket fd
		explicit socket_base(s_family const& family, s_type const& type, s_protocol const& protocol);

		virtual ~socket_base();

		inline socket_cfg const& cfg() const { return m_cfg; }
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

		inline SOCKET const& fd() const { return m_fd; }
		inline address const& remote_addr() const { return m_raddr; }
		inline address const& local_addr() const { return m_laddr; }
		
		int load_local_addr() {
			if (!m_laddr.is_null()) return wawo::OK;
			address addr;
			int rt = m_fn_getsockname(m_fd, addr);
			if (rt == wawo::OK) {
				WAWO_ASSERT(addr.family() == F_AF_INET);
				m_laddr = addr;
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

		inline bool is_nodelay() const { return ((m_cfg.option&OPTION_NODELAY) != 0); }
		inline int turnon_nodelay() { return _cfg_nodelay(true); }
		inline int turnoff_nodelay() { return _cfg_nodelay(false); }

		int turnon_nonblocking() { return _cfg_nonblocking(true); }
		int turnoff_nonblocking() { return _cfg_nonblocking(false); }
		inline bool is_nonblocking() const { return ((m_cfg.option&OPTION_NON_BLOCKING) != 0); }

		int reuse_addr() { return _cfg_reuseaddr(true); }
		int reuse_port() { return _cfg_reuseport(true); }


		//@NOTE
		// FOR PASSIVE FD , THE BEST WAY IS TO INHERITE THE SETTINGS FROM LISTEN FD
		// FOR ACTIVE FD, THE BEST WAY IS TO CALL THESE TWO APIS BEFORE ANY DATA WRITE
		int set_snd_buffer_size(u32_t const& size);
		int get_snd_buffer_size() const;
		int get_left_snd_queue() const;

		int set_rcv_buffer_size(u32_t const& size);
		int get_rcv_buffer_size() const;
		int get_left_rcv_queue() const;

		int get_linger(bool& on_off, int& linger_t) const;
		int set_linger(bool const& on_off, int const& linger_t = 30 /* in seconds */);

		int turnon_keep_alive() { return _cfg_keep_alive(true); }
		int turnoff_keep_alive() { return _cfg_keep_alive(false); }

		//@hint windows do not provide ways to retrieve idle time and interval ,and probes has been disabled by programming since vista
		int set_keep_alive_vals(keep_alive_vals const& vals) { return _cfg_keep_alive_vals(vals); }
		//int get_keep_alive_vals(keep_alive_vals& vals);

		int get_tos(u8_t& tos) const;
		int set_tos(u8_t const& tos);

		inline wawo::u32_t send(byte_t const* const buffer, wawo::u32_t const& size, int& ec_o, int const& flag = 0) {
			WAWO_ASSERT(buffer != NULL);
			WAWO_ASSERT(size > 0);
			return m_fn_send(m_fd, buffer, size, ec_o, flag);
		}
		inline wawo::u32_t recv(byte_t* const buffer_o, wawo::u32_t const& size, int& ec_o, int const& flag = 0) {
			WAWO_ASSERT(buffer_o != NULL);
			WAWO_ASSERT(size > 0);
			return m_fn_recv(m_fd, buffer_o, size, ec_o, flag);
		}

		wawo::u32_t sendto(byte_t const* const buff, wawo::u32_t const& size, const address& addr, int& ec_o, int const& flag = 0);
		wawo::u32_t recvfrom(byte_t* const buff_o, wawo::u32_t const& size, address& addr, int& ec_o);
	};
}}
#endif