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

#define WAWO_MAX_ASYNC_WRITE_PERIOD	(90000L) //90 seconds

namespace wawo { namespace net {

	enum socket_flag {
		SHUTDOWN_NONE = 0,
		SHUTDOWN_RD = 1,
		SHUTDOWN_WR = 1 << 1,
		SHUTDOWN_RDWR = (SHUTDOWN_RD | SHUTDOWN_WR),

		WATCH_READ = 1 << 2,
		WATCH_WRITE = 1 << 3,

		WATCH_OPTION_INFINITE = 1 << 4,
	};

	enum socket_state {
		S_CLOSED,
		S_OPENED,
		S_BINDED,
		S_LISTEN,
		S_CONNECTING,// for async connect
		S_CONNECTED,
	};

	struct socket_outbound_entry {
		WWRP<packet> data;
		WWRP<channel_promise> ch_promise;
	};

	typedef std::function<void(WWRP<channel>const& ch)> fn_accepted_channel_initializer;
	typedef std::function<void(WWRP<channel>const& ch)> fn_dial_channel_initializer;

#ifdef WAWO_ENABLE_IOCP
#define WAWO_IOCP_BUFFER_SIZE 1024*64
	class socket;
	enum iocp_action {
		IOCP_ACTION_IDLE,
		IOCP_ACTION_READ,
		IOCP_ACTION_WRITE,
		IOCP_ACTION_ACCEPT,
		IOCP_ACTION_LISTEN,
		IOCP_ACTION_CONNECT
	};

	struct iocp_overlapped_ctx {
		WSAOVERLAPPED _overlapped;
		WWRP<socket> so;
		WWRP<socket> ref_so;
		iocp_action action;
		WSABUF wsabuf;
		char buf[WAWO_IOCP_BUFFER_SIZE];
	};

	enum iocp_overlapped_ctx_type {
		IOCP_OVERLAPPED_CTX_ACCEPT = 0,
		IOCP_OVERLAPPED_CTX_READ,
		IOCP_OVERLAPPED_CTX_WRITE,
		IOCP_OVERLAPPED_CTX_CONNECT,
		IOCP_OVERLAPPED_CTX_MAX
	};

#endif

	class socket:
		public socket_base,
		public channel
	{
		u8_t	m_state;
		u8_t	m_rflag;
		u8_t	m_wflag;

		byte_t* m_trb; //tmp read buffer

		u64_t m_delay_wp;
		u64_t m_async_wt;
		
		fn_accepted_channel_initializer m_fn_accepted;
		std::queue<socket_outbound_entry> m_outbound_entry_q;
		u32_t m_noutbound_bytes;

#ifdef WAWO_ENABLE_IOCP
		WWSP<iocp_overlapped_ctx> m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_MAX];
#endif

		void _init();
		void _deinit();

	public:
		explicit socket(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc ,s_family const& family , s_type const& sockt, s_protocol const& proto, option const& opt = OPTION_NONE ):
			socket_base(fd,addr,sm,sbc,family, sockt,proto,opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_state(S_CONNECTED),
			m_rflag(0),
			m_wflag(0),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_noutbound_bytes(0)
		{
			_init();
		}

		explicit socket(s_family const& family, s_type const& type, s_protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(family, type, proto, opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_state(S_CLOSED),
			m_rflag(0),
			m_wflag(0),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_noutbound_bytes(0)
		{
			_init();
		}

		explicit socket(socket_buffer_cfg const& sbc, s_family const& family,s_type const& type_, s_protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(sbc, family, type_, proto, opt),
			channel(wawo::net::io_event_loop_group::instance()->next(proto == P_WCP)),
			m_state(S_CLOSED),
			m_rflag(0),
			m_wflag(0),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_noutbound_bytes(0)
		{
			_init();
		}

		~socket()
		{
			_deinit();
		}
		

#ifdef WAWO_ENABLE_IOCP
		void iocp_init() {
			iocp_init_ctx(IOCP_OVERLAPPED_CTX_READ);
			iocp_init_ctx(IOCP_OVERLAPPED_CTX_WRITE);
		}
		void iocp_deinit() {
			iocp_reset_ctx(IOCP_OVERLAPPED_CTX_READ);
			iocp_reset_ctx(IOCP_OVERLAPPED_CTX_WRITE);
		}

		void iocp_bind() {
			event_poller()->watch(IOE_IOCP_BIND, fd(), NULL, NULL, WWRP<socket>(this));
		}

		inline void iocp_init_ctx(iocp_overlapped_ctx_type t) {
			WAWO_ASSERT(m_iocp_overlapped_ctxs[t] == NULL);
			WWSP<iocp_overlapped_ctx> _ctx = wawo::make_shared<iocp_overlapped_ctx>();
			memset(&_ctx->_overlapped, 0, sizeof(WSAOVERLAPPED));
			_ctx->so = WWRP<socket>(this);
			_ctx->ref_so = NULL;
			memset(_ctx->buf, 0, WAWO_IOCP_BUFFER_SIZE);
			_ctx->action = IOCP_ACTION_IDLE;
			switch (t) {
			case IOCP_OVERLAPPED_CTX_ACCEPT:
				{
				}
				break;
			case IOCP_OVERLAPPED_CTX_READ:
				{
					memset(_ctx->buf, 0, WAWO_IOCP_BUFFER_SIZE);
				}
			break;
			case IOCP_OVERLAPPED_CTX_WRITE:
				{
					memset(_ctx->buf, 0, WAWO_IOCP_BUFFER_SIZE);
				}
			break;
			case IOCP_OVERLAPPED_CTX_CONNECT:
				{
				}
			break;
			}
			m_iocp_overlapped_ctxs[t] = _ctx;
		}

		inline WWSP<iocp_overlapped_ctx>& iocp_ctx(iocp_overlapped_ctx_type t) {
			WAWO_ASSERT(m_iocp_overlapped_ctxs[t] != NULL);
			return m_iocp_overlapped_ctxs[t];
		}

		inline void iocp_reset_ctx(iocp_overlapped_ctx_type t) {
			m_iocp_overlapped_ctxs[t]->so = NULL;
			m_iocp_overlapped_ctxs[t]->ref_so = NULL;
			m_iocp_overlapped_ctxs[t] = NULL;
		}

		inline void iocp_accepted(WWRP<channel> const& ch) {
			WAWO_ASSERT(m_fn_accepted != nullptr);
			m_fn_accepted(ch);
		}

		inline void iocp_accept_done() {
			WAWO_ASSERT(m_state == S_OPENED );
			m_sm = SM_PASSIVE;
			m_state = S_CONNECTED; //compatible for __cb_async_connected
			int rt = turnon_nonblocking();
			WAWO_ASSERT(rt == wawo::OK);

			channel::ch_fire_connected();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void iocp_read_done(int const& len) {
			if (len == 0) {
				WAWO_ASSERT("TODO");
				return;
			}
			WAWO_ASSERT(len > 0);
			WWRP<wawo::packet> income = wawo::make_ref<wawo::packet>(len);
			WAWO_ASSERT(m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_READ] != NULL);
			income->write((byte_t*)m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_READ]->buf, len);
			channel::ch_read(income);
			end_read();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void iocp_write_done(int const& len) {
			WAWO_ASSERT(m_outbound_entry_q.size());
			WAWO_ASSERT(len > 0);
			WAWO_ASSERT(m_noutbound_bytes > 0);
			socket_outbound_entry entry = m_outbound_entry_q.front();
			WAWO_ASSERT(entry.data != NULL);
			entry.data->skip(len);
			m_noutbound_bytes -= len;
			if (entry.data->len() == 0) {
				entry.ch_promise->set_success(wawo::OK);
				m_outbound_entry_q.pop();
			}
			ch_flush_impl();
		}
#endif

		int open();

		/*
		inline bool is_connected() const { return m_state == S_CONNECTED; }
		inline bool is_connecting() const { return (m_state == S_CONNECTING); }

		inline bool is_read_shutdowned() const { return (m_rflag&SHUTDOWN_RD) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&SHUTDOWN_WR) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag | m_wflag)&SHUTDOWN_RDWR) == SHUTDOWN_RDWR); }
		inline bool is_closed() const { return (m_state == S_CLOSED); }
		*/
		inline bool is_active() const { return socket_base::is_active(); }

		inline int turnon_nodelay() { return socket_base::turnon_nodelay(); }
		inline int turnoff_nodelay() { return socket_base::turnoff_nodelay(); }

		int bind(address const& addr);
		int listen(int const& backlog = 128);

		u32_t accept(WWRP<socket> sockets[], u32_t const& size, int& ec_o);
		int connect(address const& addr);

		static WWRP<channel_future> dial(std::string const& addrurl, fn_dial_channel_initializer const& initializer) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(nullptr);
			socketaddr addr;
			int rt = _parse_socketaddr_from_url(addrurl, addr);
			if (rt != wawo::OK) {
				ch_promise->set_success(rt);
				return ch_promise;
			}

			WWRP<socket> so = wawo::make_ref<socket>(addr.so_family, addr.so_type, addr.so_protocol);
			WWRP<channel_future> dial_future = so->_dial(addr.so_address, initializer);
			rt = dial_future->get();
			WAWO_RETURN_V_IF_MATCH(dial_future, rt == wawo::OK);

			//clean res in case of failure
			so->ch_close();

			ch_promise->set_success(rt);
			return ch_promise;
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
		//example
		//tcp://0.0.0.0:80
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

		inline void __cb_async_connected(WWRP<ref_base> const& fnctx ) {
			(void) fnctx;
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			m_state = S_CONNECTED;
			socket::end_connect();
			channel::ch_fire_connected();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void __cb_async_connect_error( int const& ec, WWRP<ref_base> const& fnctx) {
			(void)fnctx;
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			socket::end_connect();

			ch_errno(ec);
			ch_fire_error();
			ch_close();
		}

		void __cb_async_accept(WWRP<ref_base> const& fnctx) {
			(void)fnctx;
			WAWO_ASSERT(m_fn_accepted != NULL);
			int ec;
			do {
				WWRP<socket> accepted_sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
				u32_t count = accept(accepted_sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec);

				for (u32_t i = 0; i < count; ++i) {
					WWRP<socket>& so = accepted_sockets[i];

					int nonblocking = so->turnon_nonblocking();
					if (nonblocking != wawo::OK) {
						WAWO_ERR("[node_abstract][%s]turn on nonblocking failed: %d", so->info().to_lencstr().cstr, nonblocking);
						accepted_sockets[i]->close();
						continue;
					}

					int setdefaultkal = so->set_keep_alive_vals(default_keep_alive_vals);
					if (setdefaultkal != wawo::OK) {
						WAWO_ERR("[node_abstract][%s]set_keep_alive_vals failed: %d", so->info().to_lencstr().cstr, setdefaultkal);
						accepted_sockets[i]->close();
						continue;
					}

					m_fn_accepted(accepted_sockets[i]);
					accepted_sockets[i]->ch_fire_connected();
					accepted_sockets[i]->begin_read(WATCH_OPTION_INFINITE);
				}
			} while (ec == wawo::E_TRY_AGAIN);

			if (ec != wawo::OK) {
				ch_close();
			}
		}
		void __cb_async_read(WWRP<ref_base> const& fnctx) {
			(void)fnctx;
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(!is_listener());
			WAWO_ASSERT(is_nonblocking());

			int ec;
			do {
				WWRP<wawo::packet> arrives[10];
				u32_t count = _receive_packets(arrives, 10, ec);
				if (count > 0) {
					for (u32_t i = 0; i < count; i++) {
						channel::ch_read(arrives[i]);
					}
					if (!(m_rflag&WATCH_OPTION_INFINITE)) {
						channel::end_read();
					}
				}
			} while (ec == wawo::OK);

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
					WAWO_TRACE_SOCKET("[socket][%s]async read, pump error: %d, close", info().to_lencstr().cstr, ec);
					WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
					ch_errno(ec);
					ch_close(ch_promise);
				}
			}
		}

		void __cb_async_flush(WWRP<ref_base> const& fnctx) {
			(void)fnctx;
			socket::ch_flush_impl();
		}

		void __cb_async_error(int const& code,WWRP<ref_base> const& fnctx) {
			(void)fnctx;
			WAWO_WARN("[socket][%s]socket error: %d, close", info().to_lencstr().cstr, code);
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
			ch_close(ch_promise);
		}

		inline void __rdwr_check() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			u8_t nflag = 0;
			if (m_rflag& SHUTDOWN_RD) {
				nflag |= SHUTDOWN_RD;
			}
			
			if (m_wflag& SHUTDOWN_WR) {
				nflag |= SHUTDOWN_WR;
			}
			if (nflag == SHUTDOWN_RDWR) {
				ch_close();
			}
		}

		inline u32_t _receive_packets(WWRP<wawo::packet> arrives[], u32_t const& size, int& ec_o) {
			u32_t n = 0;
			do {
				u32_t nbytes = socket_base::recv(m_trb, buffer_cfg().rcv_size, ec_o);
				if (nbytes > 0) {
					WWRP<packet> p = wawo::make_ref<packet>(nbytes);
					p->write(m_trb, nbytes);
					arrives[n++] = p;
				}
			} while (ec_o == wawo::OK && n < size);
			return n;
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

			if ((m_rflag&WATCH_READ) && is_nonblocking()) {
				m_rflag &= ~(WATCH_READ | WATCH_OPTION_INFINITE);
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

			if ((m_wflag&WATCH_WRITE) && is_nonblocking()) {
				m_wflag &= ~(WATCH_WRITE | WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_write]unwatch IOE_WRITE", info().to_lencstr().cstr );
				event_poller()->unwatch(IOE_WRITE, fd());
			}
		}

		inline void begin_listen() {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_connect();
				});
				return;
			}

			WAWO_ASSERT(is_listener());
			event_poller()->watch(IOE_LISTEN, fd(), NULL, NULL, WWRP<socket>(this) );
		}

		inline void begin_connect( fn_io_event const& fn_connected = NULL, fn_io_event_error const& fn_err = NULL ) {
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_connect();
				});
				return;
			}

			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WAWO_ASSERT(!(m_wflag&SHUTDOWN_WR));
			WAWO_ASSERT(!(m_wflag&WATCH_WRITE));
			m_wflag |= WATCH_WRITE;
			TRACE_IOE("[socket][%s][begin_connect]watch IOE_WRITE", info().to_lencstr().cstr );

			fn_io_event _fn_io= fn_connected;
			fn_io_event_error _fn_err = fn_err;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_connected, WWRP<socket>(this), std::placeholders::_1);
			}
			if (_fn_err == NULL) {
				_fn_err = std::bind(&socket::__cb_async_connect_error, WWRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			}
			event_poller()->watch( IOE_WRITE, fd(), _fn_io, _fn_err);
		}

		inline void end_connect() {
			end_write();
		}

		inline void begin_read(u8_t const& async_flag = WATCH_OPTION_INFINITE, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
			WAWO_ASSERT(is_nonblocking());
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_read();
				});
				return;
			}

			if (m_rflag&SHUTDOWN_RD) {
				TRACE_IOE("[socket][%s][begin_read]cancel for rd shutdowned already", info().to_lencstr().cstr );
				if (fn_err != NULL) {
					fn_err(wawo::E_CHANNEL_RD_SHUTDOWN_ALREADY, NULL);
				}
				return;
			}

			if (m_rflag&WATCH_READ) {
				TRACE_IOE("[socket][%s][begin_read]ignore for watch already", info().to_lencstr().cstr );
				return;
			}
			fn_io_event _fn_io = fn_read;
			fn_io_event_error _fn_err = fn_err;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_read, WWRP<socket>(this), std::placeholders::_1);
			}
			if (_fn_err == NULL) {
				_fn_err = std::bind(&socket::__cb_async_error, WWRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			}
			m_rflag |= (WATCH_READ | async_flag);
			TRACE_IOE("[socket][%s][begin_read]watch IOE_READ", info().to_lencstr().cstr );

			u8_t flag = IOE_READ;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_READ;
			}
			event_poller()->watch(flag, fd(), _fn_io, _fn_err, WWRP<socket>(this));
		}

		inline void begin_write(u8_t const& async_flag = 0, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WAWO_ASSERT(is_nonblocking());
			if (!event_poller()->in_event_loop()) {
				WWRP<socket> _so(this);
				event_poller()->execute([_so]()->void {
					_so->begin_write();
				});
				return;
			}

			if (m_wflag&SHUTDOWN_WR) {
				TRACE_IOE("[socket][%s][begin_write]cancel for wr shutdowned already", info().to_lencstr().cstr );
				if (fn_err != NULL) {
					fn_err(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY, NULL);
				}
				return;
			}

			if (m_wflag&WATCH_WRITE) {
				TRACE_IOE("[socket][%s][begin_write]ignore for write watch already", info().to_lencstr().cstr );
				return;
			}

			fn_io_event _fn_io = fn_write;
			fn_io_event_error _fn_err = fn_err;
			if (_fn_io == NULL) {
				_fn_io = std::bind(&socket::__cb_async_flush, WWRP<socket>(this), std::placeholders::_1);
			}
			if (_fn_err == NULL) {
				_fn_err = std::bind(&socket::__cb_async_error, WWRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			}

			m_wflag |= (WATCH_WRITE | async_flag);
			TRACE_IOE("[socket][%s][begin_write]watch IOE_WRITE", info().to_lencstr().cstr );

			u8_t flag = IOE_WRITE;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_WRITE;
			}

			event_poller()->watch(flag,fd(), _fn_io, _fn_err, WWRP<socket>(this));
		}

		inline int ch_id() const { return fd(); }
		void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_wflag&SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}

			if (WAWO_UNLIKELY(m_noutbound_bytes>0)) {
				WAWO_ASSERT(m_outbound_entry_q.size()>0);
				if (m_noutbound_bytes + outlet->len() > buffer_cfg().snd_size ) {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
					return;
				}
				m_outbound_entry_q.push({
					outlet,
					ch_promise
				});
				m_noutbound_bytes += outlet->len();
				return;
			}

			if (ch_promise->is_cancelled()) {
				return;
			}

#ifdef WAWO_ENABLE_IOCP
			m_outbound_entry_q.push({
				outlet,
				ch_promise
			});
			m_noutbound_bytes += outlet->len();
			ch_flush_impl();
#else
			WAWO_ASSERT((m_wflag&WATCH_WRITE) == 0);
			int _errno;
			u32_t sent = socket_base::send(outlet->begin(), outlet->len(), _errno);
			if (sent == outlet->len()) {
				WAWO_ASSERT(_errno == wawo::OK);
				ch_promise->set_success(_errno);
				return;
			}

			if (_errno == wawo::E_CHANNEL_WRITE_BLOCK) {
				outlet->skip(sent);
				WAWO_ASSERT(m_noutbound_bytes == 0);
				m_noutbound_bytes = outlet->len();
				m_outbound_entry_q.push({
					outlet,
					ch_promise
				});
				begin_write();
				channel::ch_fire_write_block();
				return;
			}

			ch_close();
			ch_promise->set_success(_errno);
#endif
		}

		void ch_flush_impl()
		{

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

#ifdef WAWO_ENABLE_IOCP
				WWRP<packet>& outlet = entry.data;
				WWSP<iocp_overlapped_ctx>& wctx = m_iocp_overlapped_ctxs[IOCP_OVERLAPPED_CTX_WRITE];
				WAWO_ASSERT(wctx != NULL);
				if (wctx->action == IOCP_ACTION_WRITE) {
					return;
				}
				wctx->action = IOCP_ACTION_WRITE;
				memset(&wctx->_overlapped, 0, sizeof(wctx->_overlapped));
				WSABUF wsabuf = { outlet->len(), (char*)outlet->begin() };
				int wrt = ::WSASend(fd(), &wsabuf, 1, NULL, 0, &wctx->_overlapped, NULL);
				if (wrt == SOCKET_ERROR) {
					_errno = wawo::socket_get_last_errno();
					if (_errno == WSA_IO_PENDING) {
						_errno = wawo::OK;
						goto _clean_outbound_entry_q;
					}
				}
			}
			return;
#else
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
			if (m_wflag&WATCH_WRITE) {
				end_write();
				channel::ch_fire_write_unblock();
			}
			return;
		} else if (_errno == wawo::E_CHANNEL_WRITE_BLOCK ) {
			if ((m_wflag&WATCH_WRITE) == 0) {
				begin_write();
				channel::ch_fire_write_block();
			} else {
				//clear old before setup
				end_write();
				begin_write();
			}
			return;
		} else {
			goto _clean_outbound_entry_q;
		}
#endif
		_clean_outbound_entry_q:
			while (m_outbound_entry_q.size()) {
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				entry.ch_promise->set_success(_errno);
				m_outbound_entry_q.pop();
			}
			ch_close();
		}

		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_rflag&SHUTDOWN_RD) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			m_rflag |= SHUTDOWN_RD;
			int rt = socket_base::shutdown(SHUT_RD);
			end_read();
			channel::ch_fire_read_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}
		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_wflag&SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			ch_flush_impl();
			m_wflag |= SHUTDOWN_WR;
			int rt = socket_base::shutdown(SHUT_WR);
			end_write();
			channel::ch_fire_write_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}

		void ch_close_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (m_state == S_CLOSED) {
				ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				return;
			}

			int rt = socket_base::close();
			if (is_listener()) {
				end_read();
				m_state = S_CLOSED;
				ch_promise->set_success(rt);
				channel::ch_close_promise()->set_success(rt);
				channel::ch_fire_closed();
				channel::ch_close_future()->reset();
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
			if (!(m_rflag&SHUTDOWN_RD)) {
				m_rflag |= SHUTDOWN_RD;
				channel::ch_fire_read_shutdowned();
				end_read();
			}

			if (!(m_wflag&SHUTDOWN_WR)) {
				ch_flush_impl();
				m_wflag |= SHUTDOWN_WR;
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