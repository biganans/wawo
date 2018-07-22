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

#if defined(WAWO_PLATFORM_WIN) && defined(WAWO_ENABLE_IOCP)
//	#define DEFAULT_LISTEN_BACKLOG SOMAXCONN
	#define DEFAULT_LISTEN_BACKLOG 512
#else
	#define WAWO_DEFAULT_LISTEN_BACKLOG 256
#endif

namespace wawo { namespace net {
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

	class socket:
		public socket_base,
		public channel
	{
		u16_t m_flag;
		u16_t m_state;

		byte_t* m_trb; //tmp read buffer

		fn_channel_initializer m_fn_accept_initializer;
		fn_channel_initializer m_fn_dial_initializer;

		WWRP<channel_promise> m_dial_promise;
		WWRP<channel_promise> m_shutdown_write_promise;
		WWRP<channel_promise> m_close_promise;

		std::queue<socket_outbound_entry> m_outbound_entry_q;
		wawo::size_t m_noutbound_bytes;

#ifdef WAWO_IO_MODE_IOCP
		WSAOVERLAPPED* m_ol_write;
#endif
		void _init();
		void _deinit();

	public:
		explicit socket(SOCKET const& fd, socket_mode const& sm, address const& laddr, address const& raddr,s_family const& family , s_type const& sockt, s_protocol const& proto):
			socket_base(fd,sm,laddr,raddr,family, sockt,proto),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_flag(0),
			m_state(S_CONNECTED),
			m_trb(NULL),
			m_noutbound_bytes(0)
#ifdef WAWO_IO_MODE_IOCP
			,m_ol_write(0)
#endif
		{
			ch_fire_open(); //may throw
		}

		int init(socket_cfg const& cfg) {
			int rt = socket_base::init(cfg);
			WAWO_RETURN_V_IF_MATCH(rt, rt == wawo::E_SOCKET_ERROR);
			_init();
			return wawo::OK;
		}

		explicit socket(s_family const& family, s_type const& type, s_protocol const& proto) :
			socket_base(family, type, proto),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_flag(0),
			m_state(S_CLOSED),
			m_trb(NULL),
			m_noutbound_bytes(0)
#ifdef WAWO_IO_MODE_IOCP
			, m_ol_write(0)
#endif
		{
		}

		~socket()
		{
			_deinit();
		}

		int open(socket_cfg const& cfg = default_socket_cfg );
		int bind(address const& addr);
		int listen(socket_cfg const& child_cfg, int const& backlog = WAWO_DEFAULT_LISTEN_BACKLOG);

		SOCKET accept(address& raddr);
		int connect(address const& addr);

		inline bool is_active() const { return socket_base::is_active(); }
		int ch_set_read_buffer_size(u32_t size) {
			return socket_base::set_rcv_buffer_size(size);
		}
		int ch_get_read_buffer_size(u32_t& size) {
			return socket_base::get_rcv_buffer_size(size);
		}
		int ch_set_write_buffer_size(u32_t size) {
			return socket_base::set_snd_buffer_size(size);
		}
		int ch_get_write_buffer_size(u32_t& size) {
			return socket_base::get_snd_buffer_size(size);
		}

		static WWRP<channel_future> dial(std::string const& dialurl, fn_channel_initializer const& initializer, socket_cfg const& cfg = default_socket_cfg ) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(nullptr);
			socketaddr soaddr;
			int rt = _parse_socketaddr_from_url(dialurl, soaddr);
			if (rt != wawo::OK) {
				ch_promise->set_success(rt);
				return ch_promise;
			}

			WWRP<socket> so = wawo::make_ref<socket>(soaddr.so_address.family(), soaddr.so_type, soaddr.so_proto);
			WWRP<channel_promise> dial_future = so->make_promise();
			dial_future->add_listener([](WWRP<channel_future> f) {
				if (f->get() != wawo::OK) {
					f->channel()->ch_close();
				}
			});
			so->_dial(soaddr.so_address, initializer, dial_future, cfg);
			return dial_future;
		}

		/*
		 example:
			socket::listen("tcp://127.0.0.1:80", [](WWRP<channel> const& ch) {
				WWRP<wawo::net::channel_handler_abstract> h = wawo::make_ref<wawo::net::handler::echo>():
				ch->pipeline()->add_last(h);
			});
		*/
		static WWRP<channel_future> listen_on(std::string const& addrurl, fn_channel_initializer const& accepted, socket_cfg const& cfg = default_socket_cfg, socket_cfg const& child_cfg = default_socket_cfg, int backlog = WAWO_DEFAULT_LISTEN_BACKLOG) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(nullptr);
			socketaddr soaddr;
			int rt = _parse_socketaddr_from_url(addrurl, soaddr);
			if (rt != wawo::OK) {
				ch_promise->set_success(rt);
				return ch_promise;
			}

			WWRP<socket> so = wawo::make_ref<socket>(soaddr.so_address.family(), soaddr.so_type, soaddr.so_proto);
			WWRP<channel_future> listen_future = so->_listen_on(soaddr.so_address, accepted, cfg, child_cfg, backlog);
			rt = listen_future->get();
			WAWO_RETURN_V_IF_MATCH(listen_future, rt == wawo::OK);

			//clean res in case of failure
			WAWO_ASSERT(rt == wawo::E_SOCKET_ERROR);
			rt = wawo::socket_get_last_errno();

			so->ch_errno(rt);
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
				addr.so_type = T_STREAM;
				addr.so_proto = P_TCP;
			}
			else if (_arr[0] == "wcp") {
				addr.so_type = T_DGRAM;
				addr.so_proto = P_WCP;
			}
			else if (_arr[0] == "udp") {
				addr.so_type = T_STREAM;
				addr.so_proto = P_UDP;
			}
			else {
				return wawo::E_SOCKET_INVALID_PROTOCOL;
			}

			if (_arr[1].substr(0, 2) != "//") {
				return wawo::E_SOCKET_INVALID_ADDRESS;
			}
			std::string ip = _arr[1].substr(2);
			if (!is_dotipv4_decimal_notation(ip.c_str())) {
				return wawo::E_SOCKET_UNKNOWN_ADDRESS_FORMAT;
			}
			addr.so_address = address( ip.c_str(), wawo::to_u32(_arr[2].c_str()) & 0xFFFF, wawo::net::F_AF_INET);
			return wawo::OK;
		}

		WWRP<channel_future> _listen_on(address const& addr, fn_channel_initializer const& fn_accepted, socket_cfg const& cfg, socket_cfg const&child_cfg = default_socket_cfg, int const& backlog = WAWO_DEFAULT_LISTEN_BACKLOG);
		void _listen_on(address const& addr, fn_channel_initializer const& fn_accepted, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg, socket_cfg const& child_cfg = default_socket_cfg, int const& backlog = WAWO_DEFAULT_LISTEN_BACKLOG);

		WWRP<channel_future> _dial(address const& addr, fn_channel_initializer const& initializer, socket_cfg const& cfg = default_socket_cfg);
		void _dial(address const& addr, fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg = default_socket_cfg);

		inline int _ch_do_shutdown_read() {
			WAWO_ASSERT((m_flag&F_READ_SHUTDOWN) == 0);
			end_read();
			m_flag |= F_READ_SHUTDOWN;
			return socket_base::shutdown(SHUT_RD);
		}
		inline int _ch_do_shutdown_write() {
			WAWO_ASSERT((m_flag&F_WRITE_SHUTDOWN) == 0);
			while (m_outbound_entry_q.size()) {
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				m_noutbound_bytes -= entry.data->len();
				event_poller()->schedule([entry]() {
					entry.ch_promise->set_success(wawo::E_CHANNEL_CANCEL_WRITE);
				});
				m_outbound_entry_q.pop();
			}
			end_write();
			m_flag |= F_WRITE_SHUTDOWN;
			return socket_base::shutdown(SHUT_WR);
		}

		inline void __cb_async_connect(async_io_result const& r) {
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
#ifndef WAWO_IO_MODE_IOCP
			socket::end_connect();
#endif
			int code = r.v.code;
			if (code == wawo::OK && local_addr() == remote_addr()) {
				code = wawo::E_WSAEADDRNOTAVAIL;
			}

			WAWO_ASSERT(m_state == S_CONNECTING);
			WAWO_ASSERT(m_dial_promise != NULL);
			//set_success may cause close which may result in self set null
			WWRP<channel_promise> _ch_p = m_dial_promise;
			m_dial_promise = NULL;
			try {
				_ch_p->set_success(code);
			} catch (...) {
			}

			if ( WAWO_LIKELY(code == wawo::OK)) {
				WAWO_ASSERT(m_fn_dial_initializer != NULL);
				m_fn_dial_initializer(WWRP<channel>(this));

				m_fn_dial_initializer = NULL;
				m_state = S_CONNECTED;
				channel::ch_fire_connected();
				begin_read();
			} else {
				ch_errno(code);
				ch_close();
			}
		}

		inline void __new_fd(SOCKET nfd, address const& laddr, address& raddr) {
			try {
				WWRP<socket> so = wawo::make_ref<socket>(nfd, SM_PASSIVE, laddr, raddr, sock_family(), sock_type(), sock_protocol() );
				int rt = so->init(m_child_cfg);
				if (rt == wawo::E_SOCKET_ERROR) {
					so->ch_close();
					WAWO_ERR("[#%d]accept new fd error, fd: %d, errno: %d", fd(), so->ch_id(), wawo::socket_get_last_errno() );
					return;
				}
				m_fn_accept_initializer(so);
				so->ch_fire_connected();
#ifdef WAWO_IO_MODE_IOCP
				so->__IOCP_init();
#endif
				so->begin_read();
			}
			catch (std::exception& e) {
				WAWO_ERR("[#%d]accept new fd exception, e: %s", fd(), e.what());
				WAWO_CLOSE_SOCKET(nfd);
			}
			catch (wawo::exception& e) {
				WAWO_ERR("[#%d]accept new fd exception: [%d]%s\n%s(%d) %s\n%s", fd(),
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				WAWO_CLOSE_SOCKET(nfd);
			}
			catch (...) {
				WAWO_ERR("[#%d]accept new fd exception, e: %d", fd(), wawo::socket_get_last_errno());
				WAWO_CLOSE_SOCKET(nfd);
			}
		}

		void __cb_async_accept( async_io_result const& r ) {
			WAWO_ASSERT(m_fn_accept_initializer != NULL);
			int ec = r.v.code;

#ifdef WAWO_IO_MODE_IOCP
			WAWO_ASSERT(r.op == AIO_ACCEPT);
			if (ec > 0) {
				int nfd = ec;
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

				if (raddr_in == laddr_in) {
					WAWO_CLOSE_SOCKET(r.v.fd);
				} else {
					WAWO_ASSERT(laddr.port() == m_laddr.port());
					WAWO_ASSERT(raddr_in->sin_family == OS_DEF_family[m_family]);
					__new_fd(nfd, laddr, raddr);
				}
				ec = wawo::OK;
			}
#else
			if (ec == wawo::OK) {
				while (true) {
					address raddr;
					SOCKET nfd = socket_base::accept(raddr);
					if (nfd == SOCKET_ERROR ) {
						ec = wawo::socket_get_last_errno();
						if (ec == wawo::E_EINTR) {
							continue;
						} else {
							break;
						}
					}

					//patch for local addr
					address laddr;
					int rt = m_fn_getsockname(nfd, laddr);
					if (rt != wawo::OK) {
						WAWO_ERR("[socket][%s][accept]load local addr failed: %d", info().to_stdstring().c_str(), wawo::socket_get_last_errno());
						WAWO_CLOSE_SOCKET(nfd);
						continue;
					}

					WAWO_ASSERT(laddr.family() == m_family);
					if (laddr == raddr) {
						WAWO_CLOSE_SOCKET(nfd);
						continue;
					}
					__new_fd(nfd, laddr, raddr);
				}
#endif
			}
			if (IS_ERRNO_EQUAL_WOULDBLOCK(ec) || ec == wawo::OK) {
				return;
			}
			ch_close();
		}
		void __cb_async_read(async_io_result const& r) {
			(void)r;
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(!is_listener());
			WAWO_ASSERT(is_nonblocking());
			int ec = r.v.len;

#ifdef WAWO_IO_MODE_IOCP
			if (WAWO_LIKELY(ec) > 0) {
				WWRP<packet> income = wawo::make_ref<packet>(ec);
				income->write((byte_t*) r.buf, r.v.len );
				channel::ch_read(income);
				ec = wawo::OK;
			} else if (ec == 0) {
				ec = wawo::E_SOCKET_GRACE_CLOSE;
			} else {
				WAWO_TRACE_SOCKET("[socket][%s]WSARead error: %d", info().to_stdstring().c_str(), ec);
			}
#else
			while(ec == wawo::OK) {
				if (WAWO_UNLIKELY(m_flag&F_READ_SHUTDOWN)) { return; }
				wawo::u32_t nbytes = socket_base::recv(m_trb, m_cfg.buffer.rcv_size, ec);
				if (WAWO_LIKELY(nbytes>0)) {
					WWRP<packet> p = wawo::make_ref<packet>(nbytes);
					p->write(m_trb, nbytes);
					channel::ch_read(p);
				}
			}
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
				default:
				{
					WAWO_TRACE_SOCKET("[socket][%s]async read, error: %d, close", info().to_stdstring().c_str(), ec);
					ch_errno(ec);
					ch_close();
				}
			}
		}

		inline void __cb_async_flush(async_io_result const& r) {
			const int code = r.v.code == wawo::OK ? socket::_do_ch_flush_impl() : r.v.code;
			if (WAWO_LIKELY(code == wawo::OK
				|| code == wawo::E_CHANNEL_WRITE_BLOCK
			)) {
				return;
			}

			end_write();
			m_flag |= F_WRITE_ERROR;
			socket::ch_errno(code);
			socket::ch_close();
		}

		inline void __rdwr_shutdown_check() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ( (m_state != S_CLOSED) && (m_flag&(F_READWRITE_SHUTDOWN)) == F_READWRITE_SHUTDOWN) {
				ch_close();
			}
		}

	public:
#ifdef WAWO_IO_MODE_IOCP
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
			return ::WSASocketW( OS_DEF_family[m_family], OS_DEF_sock_type[m_type], OS_DEF_protocol[m_protocol], NULL, 0, WSA_FLAG_OVERLAPPED);
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

			addr.sin_family = OS_DEF_family[m_family];
			addr.sin_port = (m_raddr.nport());
			addr.sin_addr.s_addr = m_raddr.nipv4();
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
				return _do_ch_flush_impl();
			}
		}

		inline void __IOCP_CALL_CB_WSASend(async_io_result const& r) {
			int const& len = r.v.len;
			m_flag &= ~F_WATCH_WRITE;

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
				if (m_flag&F_CLOSE_AFTER_WRITE) {
					ch_close();
				} else if (m_flag&F_WRITE_SHUTDOWNING) {
					ch_shutdown_write();
				}else {}
				return;
			}
			ch_flush_impl();
		}

		inline void __IOCP_CALL_WSASend() {
			event_poller()->IOCP_overlapped_call( IOE_WRITE, fd(), std::bind(&socket::__IOCP_CALL_IMPL_WSASend, WWRP<socket>(this), std::placeholders::_1),
				std::bind(&socket::__IOCP_CALL_CB_WSASend, WWRP<socket>(this), std::placeholders::_1));
		}
#endif
		inline void begin_read(u8_t const& async_flag = F_WATCH_READ_INFINITE, fn_io_event const& fn_read = NULL) {
			WAWO_ASSERT(is_nonblocking());
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so,async_flag,fn_read]()->void {
					_so->begin_read(async_flag,fn_read);
				});
				return;
			}

			if (m_flag&F_WATCH_READ) {
				TRACE_IOE("[socket][%s][begin_read]ignore for watch already", info().to_stdstring().c_str());
				return;
			}

			if (m_flag&F_READ_SHUTDOWN) {
				TRACE_IOE("[socket][%s][begin_read]cancel for rd shutdowned already", info().to_stdstring().c_str());
				if (fn_read != NULL) {
					fn_read({ AIO_READ, wawo::E_CHANNEL_READ_SHUTDOWN_ALREADY , 0 });
				}
				return;
			}

			const fn_io_event _fn_io = fn_read == NULL ? std::bind(&socket::__cb_async_read, WWRP<socket>(this), std::placeholders::_1): fn_read ;
			m_flag |= (F_WATCH_READ | async_flag);
			TRACE_IOE("[socket][%s][begin_read]watch IOE_READ", info().to_stdstring().c_str());
			event_poller()->watch(IOE_READ, fd(), _fn_io);
		}

		inline void end_read() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->end_read();
				});
				return;
			}

			if ((m_flag&F_WATCH_READ) && is_nonblocking()) {
				m_flag &= ~(F_WATCH_READ | F_WATCH_READ_INFINITE);
				TRACE_IOE("[socket][%s][end_read]unwatch IOE_READ", info().to_stdstring().c_str());
				event_poller()->unwatch(IOE_READ, fd());
			}
		}

#ifndef WAWO_IO_MODE_IOCP
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

		inline void end_write() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->end_write();
				});
				return;
			}
			WAWO_ASSERT(is_nonblocking());
			if ((m_flag&F_WATCH_WRITE)) {
				m_flag &= ~(F_WATCH_WRITE);
				TRACE_IOE("[socket][%s][end_write]unwatch IOE_WRITE", info().to_stdstring().c_str());
				event_poller()->unwatch(IOE_WRITE, fd());
			}
		}

		inline void begin_connect( fn_io_event const& fn_connected = NULL ) {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so,fn_connected]()->void {
					_so->begin_connect(fn_connected);
				});
				return;
			}

			WAWO_ASSERT(m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());
			const fn_io_event _fn_io = fn_connected == NULL ? std::bind(&socket::__cb_async_connect, WWRP<socket>(this), std::placeholders::_1)  : fn_connected;

			WAWO_ASSERT(!(m_flag&F_WRITE_SHUTDOWN));
			WAWO_ASSERT(!(m_flag&F_WATCH_WRITE));
			m_flag |= F_WATCH_WRITE;
			TRACE_IOE("[socket][%s][begin_connect]watch IOE_WRITE", info().to_stdstring().c_str() );
			event_poller()->watch( IOE_WRITE, fd(), _fn_io);
		}

		inline void end_connect() {
			end_write();
		}

		inline void begin_write( fn_io_event const& fn_write = NULL ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so,fn_write]()->void {
					_so->begin_write(fn_write);
				});
				return;
			}
			if (m_flag&F_WATCH_WRITE) {
				TRACE_IOE("[socket][%s][begin_write]ignore for write watch already", info().to_stdstring().c_str());
				return;
			}

			if (m_flag&F_WRITE_SHUTDOWN) {
				TRACE_IOE("[socket][%s][begin_write]cancel for wr shutdowned already", info().to_stdstring().c_str() );
				if (fn_write != NULL) {
					fn_write({ AIO_WRITE,wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY,0 });
				}
				return;
			}

			const fn_io_event _fn_io = fn_write == NULL ? std::bind(&socket::__cb_async_flush, WWRP<socket>(this), std::placeholders::_1) : fn_write;
			m_flag |= (F_WATCH_WRITE);
			TRACE_IOE("[socket][%s][begin_write]watch IOE_WRITE", info().to_stdstring().c_str() );
			event_poller()->watch(IOE_WRITE,fd(), _fn_io );
		}
#endif

		inline channel_id_t ch_id() const { return fd(); }
		void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());

			WAWO_ASSERT(outlet->len() > 0);
			WAWO_ASSERT(ch_promise != NULL);

			if ((m_flag&F_WRITE_SHUTDOWN) != 0) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				return;
			}

			if ((m_flag&(F_WRITE_SHUTDOWNING|F_CLOSING))) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if ((m_flag&F_WRITE_BLOCKED)) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
				});
				return;
			}

			if (m_noutbound_bytes + outlet->len()>m_cfg.buffer.snd_size ) {
				m_flag |= F_WRITE_BLOCKED;
				event_poller()->schedule([ch_promise,CH=WWRP<channel>(this)]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
					CH->ch_fire_write_block();
				});
				return;
			}
			m_outbound_entry_q.push({
				outlet,
				ch_promise
			});
			m_noutbound_bytes += outlet->len();
			if(m_flag&F_WATCH_WRITE) {
				return;
			}
			ch_flush_impl();
		}

#ifdef WAWO_IO_MODE_IOCP
		inline int _do_ch_flush_impl() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT((m_flag&F_WATCH_WRITE) == 0);
			while (m_outbound_entry_q.size()) {
				WAWO_ASSERT(m_ol_write != NULL);
				WAWO_ASSERT(m_noutbound_bytes > 0);
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					m_noutbound_bytes -= entry.data->len();
					m_outbound_entry_q.pop();
					continue;
				}
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
				m_flag |= F_WATCH_WRITE;
				return wawo::OK;
			}
			return wawo::OK;
		}

		void ch_flush_impl() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (WAWO_UNLIKELY((m_flag&(F_WRITE_SHUTDOWN | F_WRITE_BLOCKED)) != 0)) { return; }
			if (m_ol_write == 0) {
				__IOCP_CALL_WSASend();
			} else {
				int rt = _do_ch_flush_impl();
				if (rt != wawo::OK) {
					ch_errno(rt);
					ch_close();
				}
			}
		}
#else
		//@note, we need simulate a async write, so for write operation, we'll flush outbound buffer in the next loop
		//flush until error
		//<0, is_error == (errno != E_CHANNEL_WRITING)
		//==0, flush done
		int _do_ch_flush_impl() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			int _errno = wawo::OK;
			WAWO_ASSERT(m_outbound_entry_q.size());
			WAWO_ASSERT(m_flag&F_WATCH_WRITE);

			//@NOTE
			//there is a rough flush algorithm to control which outlets should be flushed:
			//the maxium flush count === m_outbound_entry_q.size() at the mement
			//this is a consideration for FD flush fair
			::size_t s = m_outbound_entry_q.size();
			while ( s > 0 ) {
				WAWO_ASSERT(m_noutbound_bytes > 0);
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					m_noutbound_bytes -= entry.data->len();
					m_outbound_entry_q.pop();
					--s;
					continue;
				}

				const wawo::u32_t sent = socket_base::send(entry.data->begin(), entry.data->len(), _errno);
				WAWO_ASSERT(sent <= m_noutbound_bytes);
				m_noutbound_bytes -= sent;

				if (sent == entry.data->len()) {
					WAWO_ASSERT(_errno == wawo::OK);
					entry.ch_promise->set_success(wawo::OK);
					m_outbound_entry_q.pop();
					--s;
					continue;
				}
				entry.data->skip(sent);
				WAWO_ASSERT(entry.data->len());
				WAWO_ASSERT(_errno != wawo::OK);
				break;
			}
			//check send result
			if (_errno == wawo::OK) {
				if (m_outbound_entry_q.size() == 0) {
					if (m_flag&F_CLOSING) {
						WAWO_ASSERT(m_close_promise != NULL);
						_ch_do_close_read_write(NULL);
					}
					else if (m_flag&F_WRITE_SHUTDOWNING) {
						WAWO_ASSERT(m_shutdown_write_promise != NULL);
						int rt = _ch_do_shutdown_write();
						event_poller()->schedule([P=m_shutdown_write_promise,rt]() {
							P->set_success(rt);
						});
						m_shutdown_write_promise = NULL;
					} else {
						WAWO_ASSERT(m_shutdown_write_promise == NULL);
						WAWO_ASSERT(m_close_promise == NULL);
						end_write();
						if (m_flag&F_WRITE_BLOCKED) {
							m_flag &= ~F_WRITE_BLOCKED;
							ch_fire_write_unblock();
						}
					}
				}
			}
			else if (_errno == wawo::E_CHANNEL_WRITE_BLOCK) {
				WAWO_ASSERT(m_outbound_entry_q.size() > 0);
				if ((m_flag&F_WATCH_WRITE) == 0) {
					begin_write();
				}
			} else {
			}
			return _errno;
		}

		void ch_flush_impl() {
			WAWO_ASSERT((m_flag&F_WATCH_WRITE) == 0);
			begin_write();
		}
#endif

		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_flag&F_READ_SHUTDOWN) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				return;
			}
			int rt = _ch_do_shutdown_read();
			event_poller()->schedule([S=WWRP<socket>(this),ch_promise,rt]() {
				ch_promise->set_success(rt);
				S->ch_fire_read_shutdowned();
				S->__rdwr_shutdown_check();
			});
		}
		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(!is_listener());

			if (m_flag&F_WRITE_SHUTDOWN) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				return;
			}

			if (m_flag&F_WRITE_SHUTDOWNING) {
				WAWO_ASSERT((m_flag&F_WRITE_ERROR) == 0);
				WAWO_ASSERT(m_flag&F_WATCH_WRITE);
				WAWO_ASSERT(m_outbound_entry_q.size());
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if (m_flag&F_WATCH_WRITE) {
				WAWO_ASSERT((m_flag&F_WRITE_ERROR) == 0);
				m_flag |= F_WRITE_SHUTDOWNING;
				WAWO_ASSERT(m_outbound_entry_q.size());
				WAWO_ASSERT(m_shutdown_write_promise == NULL);
				m_shutdown_write_promise = ch_promise;
				return;
			}

			if( (m_flag&F_WRITE_ERROR) == 0 ) {
#ifdef WAWO_IO_MODE_IOCP
				if (rt == wawo::OK && (m_flag&F_WATCH_WRITE)) {
				}
#else
				WAWO_ASSERT(m_outbound_entry_q.size() == 0);
#endif
			}

			int rt =_ch_do_shutdown_write();
			event_poller()->schedule([ch_promise, S = WWRP<socket>(this), rt]() {
				ch_promise->set_success(rt);
				S->ch_fire_write_shutdowned();
				S->__rdwr_shutdown_check();
			});
		}

		inline void _ch_do_close_listener(WWRP<channel_promise> const& close_f) {
			m_state = S_CLOSED;
			end_read();
			m_fn_accept_initializer = NULL;
			int rt = socket_base::close();
			event_poller()->schedule([CH = WWRP<channel>(this), close_f, rt]() {
				close_f->set_success(rt);
				CH->ch_fire_close(rt);
			});
#ifdef WAWO_IO_MODE_IOCP
			__IOCP_deinit();
#endif
		}

		inline void _ch_do_close_connecting_ch(WWRP<channel_promise> const& close_f) {
			WAWO_ASSERT(m_fn_dial_initializer != NULL);
			m_state = S_CLOSED;
			end_write();//for connect action
			m_fn_dial_initializer = NULL;
			int rt = socket_base::close();
			event_poller()->schedule([close_f, CH = WWRP<channel>(this), rt]() {
				close_f->set_success(rt);
				CH->ch_fire_close(rt);
			});
#ifdef WAWO_IO_MODE_IOCP
			__IOCP_deinit();
#endif
		}

		inline void _ch_do_close_read_write(WWRP<channel_promise> const& close_f) {
			WAWO_ASSERT(m_state == S_CONNECTED);
			WAWO_ASSERT(!is_listener());
			m_state = S_CLOSED;
			if (!(m_flag&F_READ_SHUTDOWN)) {
				_ch_do_shutdown_read();
				event_poller()->schedule([CH = WWRP<channel>(this)]() {
					CH->ch_fire_read_shutdowned();
				});
			}

			if (!(m_flag&F_WRITE_SHUTDOWN)) {
				int rt = _ch_do_shutdown_write();
				if (m_shutdown_write_promise != NULL) {
					event_poller()->schedule([CHF=m_shutdown_write_promise,rt]() {
						CHF->set_success(rt);
					});
					m_shutdown_write_promise = NULL;
				}
				event_poller()->schedule([CH = WWRP<channel>(this)]() {
					CH->ch_fire_write_shutdowned();
				});
			}
			WAWO_ASSERT(m_outbound_entry_q.size() == 0);
			WAWO_ASSERT(m_noutbound_bytes == 0);
			int rt = socket_base::close();
			if (m_close_promise != NULL) {
				event_poller()->schedule([CHF= m_close_promise, CH = WWRP<channel>(this), rt]() {
					CHF->set_success(rt);
				});
				m_close_promise = NULL;
			}

			event_poller()->schedule([close_f,CH = WWRP<channel>(this), rt]() {
				if (close_f != NULL) {
					close_f->set_success(rt);
				}
				CH->ch_fire_close(rt);
			});

#ifdef WAWO_IO_MODE_IOCP
			__IOCP_deinit();
#endif
		}

		void ch_close_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (m_state==S_CLOSED) {
				event_poller()->schedule([ch_promise](){
					ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				});
				return;
			}

			if (is_listener()) {
				_ch_do_close_listener(ch_promise);
				return;
			}

			if (m_state == S_CONNECTING) {
				_ch_do_close_connecting_ch(ch_promise);
				return;
			}

			WAWO_ASSERT(m_state == S_CONNECTED);
			WAWO_ASSERT(m_dial_promise == NULL);
			WAWO_ASSERT(m_fn_dial_initializer == NULL);
			//wait for write done event

			if (m_flag&F_WATCH_WRITE) {
				WAWO_ASSERT((m_flag&F_WRITE_ERROR) == 0);
				m_flag |= F_CLOSING;
				m_close_promise = ch_promise;
				return;
			}

			if ((m_flag&F_WRITE_ERROR) == 0) {
#ifdef WAWO_IO_MODE_IOCP
				if ( (rt == wawo::OK) &&(m_flag&F_WATCH_WRITE)) {
#else
				WAWO_ASSERT(m_outbound_entry_q.size() == 0);
#endif
			}

			_ch_do_close_read_write(ch_promise);
		}
	};
}}
#endif