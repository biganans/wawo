#ifndef _WAWO_NET_SOCKET_HPP_
#define _WAWO_NET_SOCKET_HPP_

#include <queue>

#include <wawo/smart_ptr.hpp>
#include <wawo/time/time.hpp>
#include <wawo/mutex.hpp>

#include <wawo/packet.hpp>
#include <wawo/net/address.hpp>

#include <wawo/net/socket_base.hpp>
#include <wawo/net/channel.hpp>
#include <wawo/net/channel_future.hpp>
#include <wawo/net/io_event.hpp>

#ifdef WAWO_PLATFORM_WIN
	#define DEFAULT_LISTEN_BACKLOG SOMAXCONN
#else
	#define DEFAULT_LISTEN_BACKLOG 128
#endif

namespace wawo { namespace net {
	enum socket_flag {
		F_NONE = 0,
		F_SHUTDOWN_RD = 1,
		F_SHUTDOWN_WR = 1 << 1,
		F_SHUTDOWN_RDWR = (F_SHUTDOWN_RD | F_SHUTDOWN_WR),

		F_WATCH_READ = 1 << 2,
		F_WATCH_WRITE = 1 << 3,

		F_WATCH_OPTION_INFINITE = 1 << 4,
		F_WRITING = 1<<5,
		F_WRITE_BLOCKED = 1<<6,
		F_CLOSE_AFTER_WRITE_DONE =1<<7
	};

	enum socket_state {
		S_CLOSED,
		S_OPENED,
		S_BINDED,
		S_LISTEN,
		S_CONNECTING,// for async connect
		S_CONNECTED
	};

	struct socket_outbound_entry {
		WWRP<packet> data;
		WWRP<channel_promise> ch_promise;
	};

	typedef std::function<void(WWRP<channel>const& ch)> fn_accepted_channel_initializer;
	typedef std::function<void(WWRP<channel>const& ch)> fn_dial_channel_initializer;

	class socket:
		public socket_base,
		public channel
	{
		u8_t m_flag;
		u8_t m_state;

		byte_t* m_trb; //tmp read buffer

		fn_accepted_channel_initializer m_fn_accepted;
		WWRP<channel_promise> m_dial_promise;

		std::queue<socket_outbound_entry> m_outbound_entry_q;
		u32_t m_noutbound_bytes;

#ifdef WAWO_ENABLE_IOCP
		WSAOVERLAPPED* m_ol_write;
#endif
		void _init();
		void _deinit();

	public:
		explicit socket(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc ,s_family const& family , s_type const& sockt, s_protocol const& proto, option const& opt = OPTION_NONE ):
			socket_base(fd,addr,sm,sbc,family, sockt,proto,opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_flag(0),
			m_state(S_CONNECTED),
			m_trb(NULL),
			m_noutbound_bytes(0)
#ifdef WAWO_ENABLE_IOCP
			,m_ol_write(0)
#endif
		{
			_init();
			ch_fire_opened(); //may throw
		}

		explicit socket(s_family const& family, s_type const& type, s_protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(family, type, proto, opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_flag(0),
			m_state(S_CLOSED),
			m_trb(NULL),
			m_noutbound_bytes(0)
#ifdef WAWO_ENABLE_IOCP
			, m_ol_write(0)
#endif
		{
			_init();
		}

		explicit socket(socket_buffer_cfg const& sbc, s_family const& family,s_type const& type_, s_protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(sbc, family, type_, proto, opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_flag(0),
			m_state(S_CLOSED),
			m_trb(NULL),
			m_noutbound_bytes(0)
#ifdef WAWO_ENABLE_IOCP
			, m_ol_write(0)
#endif
		{
			_init();
		}

		~socket()
		{
			_deinit();
		}
		

		int open();

		/*
		inline bool is_connected() const { return m_state == S_CONNECTED; }
		inline bool is_connecting() const { return (m_state == S_CONNECTING); }

		inline bool is_read_shutdowned() const { return (m_rflag&F_SHUTDOWN_RD) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&F_SHUTDOWN_WR) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag | m_wflag)&SHUTDOWN_RDWR) == SHUTDOWN_RDWR); }
		inline bool is_closed() const { return (m_state == S_CLOSED); }
		*/
		inline bool is_active() const { return socket_base::is_active(); }

		inline int turnon_nodelay() { return socket_base::turnon_nodelay(); }
		inline int turnoff_nodelay() { return socket_base::turnoff_nodelay(); }

		int bind(address const& addr);
		int listen(int const& backlog = DEFAULT_LISTEN_BACKLOG);

		int accept(std::vector<WWRP<socket>>& accepted);
		int connect(address const& addr);

		static WWRP<channel_future> dial(std::string const& addrurl, fn_dial_channel_initializer const& initializer ) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(nullptr);
			socketaddr addr;
			int rt = _parse_socketaddr_from_url(addrurl, addr);
			if (rt != wawo::OK) {
				ch_promise->set_success(rt);
				return ch_promise;
			}

			WWRP<socket> so = wawo::make_ref<socket>(addr.so_family, addr.so_type, addr.so_protocol);
			WWRP<channel_future> dial_future = so->_dial(addr.so_address, initializer );

			dial_future->add_listener([](WWRP<channel_future> f) {
				if (f->get() != wawo::OK) {
					f->channel()->ch_close();
				}
			});
			return dial_future;
		}

		/*
		 example:
			socket::listen("tcp://127.0.0.1:80", [](WWRP<channel> const& ch) {
				WWRP<wawo::net::channel_handler_abstract> h = wawo::make_ref<wawo::net::handler::echo>():
				ch->pipeline()->add_last(h);
			});
		*/
		static WWRP<channel_future> listen_on(std::string const& addrurl, fn_accepted_channel_initializer const& accepted) {

			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(nullptr);
			socketaddr addr;
			int rt = _parse_socketaddr_from_url(addrurl, addr);
			if (rt != wawo::OK) {
				ch_promise->set_success(rt);
				return ch_promise;
			}

			WWRP<socket> so = wawo::make_ref<socket>(addr.so_family, addr.so_type, addr.so_protocol);
			WWRP<channel_future> listen_future = so->_listen_on(addr.so_address, accepted, 128);
			rt = listen_future->get();
			WAWO_RETURN_V_IF_MATCH(listen_future, rt == wawo::OK);

			//clean res in case of failure
			so->ch_close();

			ch_promise->set_success(rt);
			return ch_promise;
		}

	private:
		//url example: tcp://0.0.0.0:80, udp://127.0.0.1:80, wcp://54.65.109.7:11211

		static int _parse_socketaddr_from_url(std::string const& url, socketaddr& addr ) {
			std::vector<std::string> _arr;

			wawo::split(url, ":", _arr);
			if (_arr.size() != 3) {
				return wawo::E_SOCKET_INVALID_ADDRESS;
			}

			if (_arr[0] == "tcp") {
				addr.so_family = wawo::net::F_AF_INET;
				addr.so_type = T_STREAM;
				addr.so_protocol = P_TCP;
			}
			else if (_arr[0] == "wcp") {
				addr.so_family = wawo::net::F_AF_INET;
				addr.so_type = T_DGRAM;
				addr.so_protocol = P_WCP;
			}
			else if (_arr[0] == "udp") {
				addr.so_family = wawo::net::F_AF_INET;
				addr.so_type = T_DGRAM;
				addr.so_protocol = P_UDP;
			}
			else {
				return wawo::E_SOCKET_INVALID_PROTOCOL;
			}

			if (_arr[1].substr(0, 2) != "//") {
				return wawo::E_SOCKET_INVALID_ADDRESS;
			}
			std::string ip = _arr[1].substr(2);
			if (!is_ipv4_in_dotted_decimal_notation(ip.c_str())) {
				return wawo::E_SOCKET_UNKNOWN_ADDRESS_FORMAT;
			}
			addr.so_address = address( ip.c_str(), wawo::to_u32(_arr[2].c_str()) & 0xFFFF);
			return wawo::OK;
		}

		WWRP<wawo::net::channel_future> _listen_on(address const& addr, fn_accepted_channel_initializer const& fn_accepted, int const& backlog = 128);
		WWRP<wawo::net::channel_future> _listen_on(address const& addr, fn_accepted_channel_initializer const& fn_accepted, WWRP<channel_promise> const& ch_promise, int const& backlog = 128);

		WWRP<channel_future> _dial(address const& addr, fn_dial_channel_initializer const& initializer);
		WWRP<channel_future> _dial(address const& addr, fn_dial_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise);

		inline int close() {
			int rt = socket_base::close();
			m_state = S_CLOSED;
			return rt;
		}

		inline void __cb_async_connect(async_io_result const& r) {
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			socket::end_connect();

			int code = r.v.code;
			if (code == wawo::OK && local_addr() == remote_addr()) {
				code = wawo::E_WSAEADDRNOTAVAIL;
			}

			WAWO_ASSERT(m_dial_promise != NULL);
			WWRP<channel_promise> _ch_p = m_dial_promise;
			m_dial_promise = NULL;
			try {
				_ch_p->set_success(code);
			} catch (...) {
			}

			if ( WAWO_LIKELY(code == wawo::OK)) {
				m_state = S_CONNECTED;
				channel::ch_fire_connected();
				begin_read(F_WATCH_OPTION_INFINITE);
			} else {
				ch_errno(code);
				ch_close();
			}
		}

		void __cb_async_accept( async_io_result const& r ) {
			WAWO_ASSERT(m_fn_accepted != NULL);
			std::vector<WWRP<socket>> accepted;

#ifdef WAWO_ENABLE_IOCP
			WAWO_ASSERT(r.op == AIO_ACCEPT);
			int ec = r.v.code;
			if (ec > 0) {

				SOCKADDR_IN* raddr_in = NULL;
				SOCKADDR_IN* laddr_in = NULL;
				int raddr_in_len = sizeof(SOCKADDR_IN);
				int laddr_in_len = sizeof(SOCKADDR_IN);

				LPFN_GETACCEPTEXSOCKADDRS fn_getacceptexsockaddrs = (LPFN_GETACCEPTEXSOCKADDRS)winsock_helper::instance()->load_api_ex_address(API_GET_ACCEPT_EX_SOCKADDRS);
				WAWO_ASSERT(fn_getacceptexsockaddrs != 0);
				fn_getacceptexsockaddrs(r.buf, 0,
					sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
					(LPSOCKADDR*)&laddr_in, &laddr_in_len,
					(LPSOCKADDR*)&raddr_in, &raddr_in_len);

				address raddr(*raddr_in);
				address laddr(*laddr_in);

				WAWO_ASSERT(laddr.port() == m_laddr.port());
				WAWO_ASSERT(raddr_in->sin_family == system_family[m_family]);
				/*
				 * @note , if the connection closed/reset by remote side before accept, we may get zero remote addr
				sockaddr_in addr_in;
				char ipstr[INET6_ADDRSTRLEN] = { 0 };
				socklen_t addr_len = sizeof(addr_in);
				int rt = ::getpeername(r.v.fd, (struct sockaddr*)&addr_in, &addr_len);
				if (rt == -1) {
					WAWO_ERR("[#%d]getpeername failed: %d, close socket", r.v.fd, wawo::get_last_errno());
					WAWO_CLOSE_SOCKET(r.v.fd);
					return;
				}
				WAWO_ASSERT(addr_in.sin_family == system_family[m_family]);
				struct sockaddr_in *s = (struct sockaddr_in *)&addr_in;
				unsigned short port = ::ntohs(s->sin_port);
				::inet_ntop(system_family[m_family], &s->sin_addr, ipstr, sizeof(ipstr));				
				address _raddr(ipstr, port);
				*/
				try {
					WWRP<socket> so = wawo::make_ref<socket>(r.v.fd, raddr, SM_PASSIVE, buffer_cfg(), sock_family(), sock_type(), sock_protocol(), OPTION_NONE);
					accepted.push_back(so);
				} catch (std::exception& e) {
					WAWO_ERR("[#%d]accept new fd exception, e: %s", fd(), e.what());
					WAWO_CLOSE_SOCKET(r.v.fd);
				} catch (wawo::exception& e) {
					WAWO_ERR("[#%d]accept new fd exception: [%d]%s\n%s(%d) %s\n%s", fd(),
						e.code, e.message, e.file, e.line, e.func, e.callstack);
					WAWO_CLOSE_SOCKET(r.v.fd);
				} catch (...) {
					WAWO_ERR("[#%d]accept new fd exception, e: %d", fd(),wawo::socket_get_last_errno());
					WAWO_CLOSE_SOCKET(r.v.fd);
				}
				ec = wawo::OK;
			}
#else
			int ec = accept(accepted);
#endif

			std::size_t count = accepted.size();

			for (std::size_t i = 0; i < count; ++i) {
				WWRP<socket>& so = accepted[i];

				int nonblocking = so->turnon_nonblocking();
				if (nonblocking != wawo::OK) {
					WAWO_ERR("[node_abstract][%s]turn on nonblocking failed: %d", so->info().to_lencstr().cstr, nonblocking);
					accepted[i]->close();
					continue;
				}

				int setdefaultkal = so->set_keep_alive_vals(default_keep_alive_vals);
				if (setdefaultkal != wawo::OK) {
					WAWO_ERR("[node_abstract][%s]set_keep_alive_vals failed: %d", so->info().to_lencstr().cstr, setdefaultkal);
					accepted[i]->close();
					continue;
				}

				m_fn_accepted(accepted[i]);
				accepted[i]->ch_fire_connected();
#ifdef WAWO_ENABLE_IOCP
				accepted[i]->__IOCP_init();
#endif
				accepted[i]->begin_read(F_WATCH_OPTION_INFINITE);
			}

			if (ec != wawo::OK) {
				ch_close();
			}
		}
		void __cb_async_read(async_io_result const& r) {
			(void)r;
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(!is_listener());
			WAWO_ASSERT(is_nonblocking());

#ifdef WAWO_ENABLE_IOCP
			int ec = r.v.len;
			if (WAWO_LIKELY(ec) > 0) {
				WWRP<packet> income = wawo::make_ref<packet>(ec);
				income->write((byte_t*) r.buf, r.v.len );
				channel::ch_read(income);
				ec = wawo::OK;
			} else if (ec == 0) {
				ec = wawo::E_SOCKET_GRACE_CLOSE;
			} else {
				WAWO_TRACE_SOCKET("[socket][%s]WSARead error: %d", info().to_lencstr().cstr, ec);
			}
#else
			int ec = _receive_packets();
#endif
			switch (ec) {
				case wawo::OK:
				case wawo::E_CHANNEL_READ_BLOCK:
				{}
				break;
				case wawo::E_SOCKET_GRACE_CLOSE:
				{
					ch_shutdown_read();
				}
				break;
				case wawo::E_CHANNEL_RD_SHUTDOWN_ALREADY:
				{
				}
				break;
				default:
				{
					WAWO_TRACE_SOCKET("[socket][%s]async read, error: %d, close", info().to_lencstr().cstr, ec);
					ch_errno(ec);
					ch_close();
				}
			}
		}

		void __cb_async_flush(async_io_result const& r) {
			(void)r;
			socket::ch_flush_impl();
		}

		inline void __rdwr_check() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_flag&(F_SHUTDOWN_RDWR)) == F_SHUTDOWN_RDWR) {
				ch_close();
			}
		}

		inline int _receive_packets() {
			int ec;
			do {
				u32_t nbytes = socket_base::recv(m_trb, buffer_cfg().rcv_size, ec);
				if (nbytes>0) {
					WWRP<packet> p = wawo::make_ref<packet>(nbytes);
					p->write(m_trb, nbytes);
					channel::ch_read(p);
				}
			} while (ec == wawo::OK);
			return ec;
		}

	public:
		inline void end_read() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->end_read();
				});
				return;
			}

			if ((m_flag&F_WATCH_READ) && is_nonblocking()) {
				m_flag &= ~(F_WATCH_READ | F_WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_read]unwatch IOE_READ", info().to_lencstr()().cstr);
				event_poller()->unwatch(IOE_READ, fd() );
			}
		}

		inline void end_write() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->end_write();
				});
				return;
			}

			if ((m_flag&F_WATCH_WRITE) && is_nonblocking()) {
				m_flag &= ~(F_WATCH_WRITE | F_WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_write]unwatch IOE_WRITE", info().to_lencstr().cstr );
				event_poller()->unwatch(IOE_WRITE, fd());
			}
		}

#ifdef WAWO_ENABLE_IOCP
		inline void __IOCP_init() {
			event_poller()->watch(IOE_IOCP_INIT, fd(), std::bind(&socket::__cb_async_accept, WWRP<socket>(this), std::placeholders::_1));
		}

		inline void __IOCP_deinit() {
			event_poller()->watch(IOE_IOCP_DEINIT, fd(), NULL);
		}

		inline void __IOCP_CALL_AcceptEx(fn_io_event const& cb_connected = NULL) {
			__IOCP_init();
			fn_io_event _fn_io = cb_connected;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_accept, WWRP<socket>(this), std::placeholders::_1);
			}
			event_poller()->IOCP_overlapped_call(IOE_ACCEPT, fd(), std::bind(&socket::__IOCP_CALL_AcceptEx_IMPL, WWRP<socket>(this), std::placeholders::_1), _fn_io);
		}

		inline int __IOCP_CALL_AcceptEx_IMPL(void* ol_) {
			(void)ol_;
			return __IOCP_CALL_IMPL_WSASocket();
		}

		inline int __IOCP_CALL_IMPL_WSASocket() {
			return ::WSASocketW( system_family[m_family], system_sock_type[m_type], system_protocol[m_protocol], NULL, 0, WSA_FLAG_OVERLAPPED);
		}

		inline void __IOCP_CALL_ConnectEx(fn_io_event const& cb_connected = NULL) {
			__IOCP_init();
			fn_io_event _fn_io = cb_connected;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_connect, WWRP<socket>(this), std::placeholders::_1);
			}
			event_poller()->IOCP_overlapped_call(IOE_CONNECT, fd(), std::bind(&socket::__IOCP_CALL_IMPL_ConnectEx, WWRP<socket>(this), std::placeholders::_1), _fn_io);
		}

		inline int __IOCP_CALL_IMPL_ConnectEx(void* ol_) {
			WAWO_ASSERT(ol_ != NULL);
			WSAOVERLAPPED* ol = (WSAOVERLAPPED*)ol_;
			WAWO_ASSERT(!m_raddr.is_null());

			sockaddr_in addr;
			::memset(&addr, 0,sizeof(addr));

			addr.sin_family = system_family[m_family];
			addr.sin_port = m_raddr.nport();
			addr.sin_addr.s_addr = m_raddr.nip();
			int socklen = sizeof(addr);
			LPFN_CONNECTEX fn_connectEx = (LPFN_CONNECTEX)winsock_helper::instance()->load_api_ex_address(API_CONNECT_EX);
			WAWO_ASSERT(fn_connectEx != 0);

			BOOL connrt = fn_connectEx(fd(),(SOCKADDR*)(&addr), socklen,NULL, 0, NULL, ol);
			if (connrt == TRUE) {
				WAWO_DEBUG("[#%d]connectex return ok immediately", fd() );
			} else {
				int ec = wawo::socket_get_last_errno();
				if (ec != wawo::E_ERROR_IO_PENDING) {
					WAWO_DEBUG("[#%d]socket __connectEx failed", fd(), connrt);
					return ec;
				}
			}
			
			return wawo::OK;
		}

		//non null, send
		//null clear
		inline int __IOCP_CALL_IMPL_WSASend( void* ol_ ) {
			WSAOVERLAPPED* ol = (WSAOVERLAPPED*)ol_;
			if (ol == 0) {
				WAWO_ASSERT(m_ol_write == 0);
				m_ol_write = ol;
				return wawo::OK;
			} else {
				m_ol_write = ol;
				return __IOCP_CALL_MY_WSASend();
			}
		}

		inline int __IOCP_CALL_MY_WSASend() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(m_ol_write != NULL );
			while (m_outbound_entry_q.size()) {
				WAWO_ASSERT(m_noutbound_bytes > 0);
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					m_noutbound_bytes -= entry.data->len();
					m_outbound_entry_q.pop();
					continue;
				}
				if (m_flag&F_WRITING) { return wawo::E_EALREADY; }
				WWRP<packet>& outlet = entry.data;
				::memset(m_ol_write, 0, sizeof(*m_ol_write));
				WSABUF wsabuf = { outlet->len(), (char*)outlet->begin() };
				int wrt = ::WSASend(fd(), &wsabuf, 1, NULL, 0, m_ol_write, NULL);
				if (wrt == SOCKET_ERROR) {
					wrt = wawo::socket_get_last_errno();
					if (wrt != wawo::E_ERROR_IO_PENDING) {
						WAWO_DEBUG("[#%d]socket __WSASend failed", fd(), wrt);
						return wrt;
					}
				}
				m_flag |= F_WRITING;
				return wawo::OK;
			}
			return wawo::OK;
		}

		inline void __IOCP_CALL_CB_WSASend(async_io_result const& r) {
			int const& len = r.v.len;
			m_flag &= ~F_WRITING;

			if (len<0) {
				ch_errno(len);
				ch_close();
				WAWO_DEBUG("[#%d][socket][iocp]WSASend failed: %d", fd(), len);
				return;
			}

			WAWO_ASSERT(m_outbound_entry_q.size());
			WAWO_ASSERT(m_noutbound_bytes > 0);
			socket_outbound_entry entry = m_outbound_entry_q.front();
			WAWO_ASSERT(entry.data != NULL);
			entry.data->skip(len);
			m_noutbound_bytes -= len;
			if (entry.data->len() == 0) {
				entry.ch_promise->set_success(wawo::OK);
				m_outbound_entry_q.pop();
			}
			if (m_noutbound_bytes==0) {
				if (WAWO_UNLIKELY(m_flag&F_WRITE_BLOCKED)) {
					m_flag &= ~F_WRITE_BLOCKED;
					channel::ch_fire_write_unblock();
				}
				if (m_flag&F_CLOSE_AFTER_WRITE_DONE) {
					ch_close();
				}
				return;
			}
			ch_flush_impl();
		}

		inline void __IOCP_CALL_WSASend() {
			event_poller()->IOCP_overlapped_call( IOE_WRITE, fd(), std::bind(&socket::__IOCP_CALL_IMPL_WSASend, WWRP<socket>(this), std::placeholders::_1),
				std::bind(&socket::__IOCP_CALL_CB_WSASend, WWRP<socket>(this), std::placeholders::_1));
		}
#endif

		inline void begin_accept() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_accept();
				});
				return;
			}

			WAWO_ASSERT(is_listener());
			event_poller()->watch(IOE_READ, fd(), std::bind(&socket::__cb_async_accept, WWRP<socket>(this), std::placeholders::_1));
		}

		inline void begin_connect( fn_io_event const& fn_connected = NULL ) {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_connect();
				});
				return;
			}

			WAWO_ASSERT(m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());
			fn_io_event _fn_io = fn_connected;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_connect, WWRP<socket>(this), std::placeholders::_1);
			}

			WAWO_ASSERT(!(m_flag&F_SHUTDOWN_WR));
			WAWO_ASSERT(!(m_flag&F_WATCH_WRITE));
			m_flag |= F_WATCH_WRITE;
			TRACE_IOE("[socket][%s][begin_connect]watch IOE_WRITE", info().to_lencstr().cstr );


			event_poller()->watch( IOE_WRITE, fd(), _fn_io);
		}

		inline void end_connect() {
#ifdef WAWO_ENABLE_IOCP
#else
			end_write();
#endif
		}

		inline void begin_read(u8_t const& async_flag = F_WATCH_OPTION_INFINITE, fn_io_event const& fn_read = NULL) {
			WAWO_ASSERT(is_nonblocking());
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_read();
				});
				return;
			}

			if (m_flag&F_SHUTDOWN_RD) {
				TRACE_IOE("[socket][%s][begin_read]cancel for rd shutdowned already", info().to_lencstr().cstr );
				if (fn_read != NULL) {
					fn_read({AIO_READ, wawo::E_CHANNEL_RD_SHUTDOWN_ALREADY , 0});
				}
				return;
			}

			if (m_flag&F_WATCH_READ) {
				TRACE_IOE("[socket][%s][begin_read]ignore for watch already", info().to_lencstr().cstr );
				return;
			}
			fn_io_event _fn_io = fn_read;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_read, WWRP<socket>(this), std::placeholders::_1);
			}
			m_flag |= (F_WATCH_READ | async_flag);
			TRACE_IOE("[socket][%s][begin_read]watch IOE_READ", info().to_lencstr().cstr );

			u8_t flag = IOE_READ;
			if (async_flag&F_WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_READ;
			}
			event_poller()->watch(flag, fd(), _fn_io);
		}

		inline void begin_write(u8_t const& async_flag = 0, fn_io_event const& fn_write = NULL ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_write();
				});
				return;
			}

			if (m_flag&F_SHUTDOWN_WR) {
				TRACE_IOE("[socket][%s][begin_write]cancel for wr shutdowned already", info().to_lencstr().cstr );
				if (fn_write != NULL) {
					fn_write({ AIO_WRITE,wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY,0 });
				}
				return;
			}

			if (m_flag&F_WATCH_WRITE) {
				TRACE_IOE("[socket][%s][begin_write]ignore for write watch already", info().to_lencstr().cstr );
				return;
			}

			fn_io_event _fn_io = fn_write;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_flush, WWRP<socket>(this), std::placeholders::_1);
			}

			m_flag |= (F_WATCH_WRITE | async_flag);
			TRACE_IOE("[socket][%s][begin_write]watch IOE_WRITE", info().to_lencstr().cstr );

			u8_t flag = IOE_WRITE;
			if (async_flag&F_WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_WRITE;
			}

			event_poller()->watch(flag,fd(), _fn_io );
		}

		inline int ch_id() const { return fd(); }
		void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (ch_promise->is_cancelled()) {
				return;
			}
			if ((m_flag&F_SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}

#ifdef WAWO_ENABLE_IOCP
			if ((m_flag&F_WRITE_BLOCKED)) {
				ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
				return;
			}
#endif

			if (m_noutbound_bytes + outlet->len()>buffer_cfg().snd_size ) {
				ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
#ifdef WAWO_ENABLE_IOCP
				m_flag |= F_WRITE_BLOCKED;
				channel::ch_fire_write_block();
#endif
				return;
			}
			m_outbound_entry_q.push({
				outlet,
				ch_promise
			});
			m_noutbound_bytes += outlet->len();
			ch_flush_impl();
		}

		void ch_flush_impl()
		{
#ifdef WAWO_ENABLE_IOCP
			if (m_ol_write == 0) {
				__IOCP_CALL_WSASend();
			} else {
				int rt = __IOCP_CALL_MY_WSASend();
				if (rt != wawo::OK) {
					ch_errno(rt);
					ch_close();
				}
			}
			return;
#else
			if (WAWO_UNLIKELY((m_flag&(F_SHUTDOWN_WR|WRITE_BLOCKED)) != 0)) { return; }

			WAWO_ASSERT(event_poller()->in_event_loop());
			int _errno = 0;
			while (m_outbound_entry_q.size()) {
				WAWO_ASSERT(m_noutbound_bytes > 0);
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					m_noutbound_bytes -= entry.data->len();
					m_outbound_entry_q.pop();
					continue;
				}

				u32_t sent = socket_base::send(entry.data->begin(), entry.data->len(), _errno);
				WAWO_ASSERT(sent <= m_noutbound_bytes);
				m_noutbound_bytes -= sent;

				if (sent == entry.data->len()) {
					WAWO_ASSERT(_errno == wawo::OK);
					entry.ch_promise->set_success(wawo::OK);
					m_outbound_entry_q.pop();
					continue;
				}
				entry.data->skip(sent);
				WAWO_ASSERT(entry.data->len());
				WAWO_ASSERT(_errno != wawo::OK);
				break;
			}
			if (_errno == wawo::OK) {
				WAWO_ASSERT(m_outbound_entry_q.size() == 0);
				if (m_flag&F_WATCH_WRITE) {
					end_write();
					m_flag &= ~F_WRITE_BLOCKED;
					channel::ch_fire_write_unblock();
				}
				return;
			} else if (_errno == wawo::E_CHANNEL_WRITE_BLOCK ) {
				if ((m_flag&F_WATCH_WRITE) == 0) {
					begin_write();
					m_flag |= F_WRITE_BLOCKED;
					channel::ch_fire_write_block();
				} else {
					//@TODO clear old before setup, this is just a quick fix
					end_write();
					begin_write();
				}
				return;
			}
			ch_close();
#endif
		}

		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_flag&F_SHUTDOWN_RD) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			end_read();
			m_flag |= F_SHUTDOWN_RD;
			int rt = socket_base::shutdown(SHUT_RD);
			channel::ch_fire_read_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}
		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_flag&F_SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			ch_flush_impl();
			end_write();
			m_flag |= F_SHUTDOWN_WR;
			int rt = socket_base::shutdown(SHUT_WR);
			channel::ch_fire_write_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}

		void ch_close_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (m_state==S_CLOSED) {
				ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				return;
			}

			WAWO_ASSERT(m_dial_promise == NULL);

#ifdef WAWO_ENABLE_IOCP
			if (m_flag&F_WRITING) {
				WAWO_ASSERT(!is_listener());
				m_flag |= F_CLOSE_AFTER_WRITE_DONE;
				if (!(m_flag&F_SHUTDOWN_RD)) {
					ch_shutdown_read();
				}
				return;
			}
			__IOCP_deinit();
#else
			if (!(m_wflag&F_SHUTDOWN_WR) && !is_listener() && m_state == S_CONNECTED) {
				ch_flush_impl();
			}
#endif
			int rt = socket_base::close();
			if (is_listener()) {
				end_read();
				m_state = S_CLOSED;
				ch_promise->set_success(rt);
				channel::ch_close_promise()->set_success(rt);
				channel::ch_fire_closed();
				channel::ch_close_future()->reset();
				m_fn_accepted = NULL;
				return;
			}

			if (m_state != S_CONNECTED) {
				m_state = S_CLOSED;
				ch_promise->set_success(rt);
				channel::ch_close_promise()->set_success(rt);
				channel::ch_fire_closed();
				channel::ch_close_future()->reset();
				return;
			}

			m_state = S_CLOSED;
			while (m_outbound_entry_q.size()) {
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				entry.ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				m_outbound_entry_q.pop();
			}

			if (!(m_flag&F_SHUTDOWN_RD)) {
				m_flag |= F_SHUTDOWN_RD;
				channel::ch_fire_read_shutdowned();
				end_read();
			}

			if (!(m_flag&F_SHUTDOWN_WR)) {
				m_flag |= F_SHUTDOWN_WR;
				end_write();
			}

			ch_promise->set_success(rt);

			channel::ch_close_promise()->set_success(rt);
			channel::ch_fire_closed();
			channel::ch_close_future()->reset();
		}
	};
}}
#endif