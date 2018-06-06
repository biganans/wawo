#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void async_connected(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		WAWO_ASSERT(so->is_active());

		TRACE_IOE("[socket_base][%s][async_connected], unwatch(IOE_WRITE)", so->info().to_lencstr().cstr );
		so->end_write();
		WAWO_TRACE_SOCKET("[socket][%s]socket async connected", so->info().to_lencstr().cstr );
		so->handle_async_connected();
	}

	void async_connect_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		WAWO_ASSERT(so->is_active());

		TRACE_IOE("[socket_base][%s][async_connect_error]error code: %d, unwatch(IOE_WRITE)", so->info().to_lencstr().cstr, code);
		so->end_write();

		WAWO_TRACE_SOCKET("[socket][%s]socket async connect error", so->info().to_lencstr().cstr);
		so->handle_async_connect_error(code);
	}

	void async_accept(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		int ec;
		so->handle_async_accept(ec);
	}

	void async_read(WWRP<ref_base> const& cookie_) {

		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		int ec;

		so->handle_async_read(ec);
		switch (ec) {
			case wawo::OK:
			case wawo::E_CHANNEL_READ_BLOCK:
			{}
			break;
			case wawo::E_SOCKET_GRACE_CLOSE:
			{
				WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
				so->ch_shutdown_read(ch_promise);
			}
			break;
			case wawo::E_CHANNEL_RD_SHUTDOWN_ALREADY:
			{
			}
			break;
			default:
			{
				WAWO_TRACE_SOCKET("[socket][%s]async read, pump error: %d, close", so->info().to_lencstr().cstr, ec );
				WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
				so->ch_errno(ec);
				so->ch_close(ch_promise);
			}
		}
	}

	void async_write(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		WAWO_ASSERT(so != NULL);
		so->ch_flush_impl();
	}

	void async_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie_);
		WAWO_ASSERT(so != NULL);

		WAWO_WARN("[socket][%s]socket error: %d, close", so->info().to_lencstr().cstr, code);
		so->close();
	}
}}

namespace wawo { namespace net {

	void socket::_init() {

		m_trb = (byte_t*) ::malloc( sizeof(byte_t)*buffer_cfg().rcv_size ) ;
		WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef _DEBUG
		::memset( m_trb, 'i', buffer_cfg().rcv_size );
#endif

		m_fn_async_connected = wawo::net::async_connected;
		m_fn_async_connect_error = wawo::net::async_connect_error;

		m_fn_async_read = wawo::net::async_read;
		m_fn_async_read_error = wawo::net::async_error;

		m_fn_async_write = wawo::net::async_write;
		m_fn_async_write_error = wawo::net::async_error;
	}

	void socket::_deinit() {
		WAWO_ASSERT( m_state == S_CLOSED) ;

		::free( m_trb );
		m_trb = NULL;
	}

	int socket::open() {
		WAWO_ASSERT(m_state == S_CLOSED);
		channel::ch_opened();
		int rt = socket_base::open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		m_state = S_OPENED;
		m_rflag = 0;
		m_wflag = 0;
		return rt;
	}

	int socket::connect(address const& addr)
	{
		if (!(m_state == S_OPENED || m_state == S_BINDED)) {
			return wawo::E_INVALID_STATE;
		}
		int rt= socket_base::connect(addr);

		if (rt == wawo::OK) {
			m_state = S_CONNECTED;
			return wawo::OK;
		}

		WAWO_ASSERT(rt<0);
		if (is_nonblocking() && (IS_ERRNO_EQUAL_CONNECTING(WAWO_ABS(rt)))) {
			m_state = S_CONNECTING;
			return wawo::E_SOCKET_CONNECTING;
		}

		return rt;
	}

	void socket::ch_bind(wawo::net::address const& addr, WWRP<channel_promise>const& ch_promise) {
		WWRP<socket> _this(this);
		if (!event_loop()->in_event_loop()) {
			event_loop()->schedule([_this,addr,ch_promise]() ->void {
				_this->ch_bind(addr, ch_promise);
			});
			return;
		}

		WAWO_ASSERT(m_state == S_OPENED);

		int bindrt = socket_base::bind(addr);
		ch_promise->set_success(bindrt);
		if (bindrt == 0) {
			m_state = S_BINDED;
			WAWO_TRACE_SOCKET("[socket][%s]socket bind ok", info().to_lencstr().cstr );
			return;
		}
		WAWO_ASSERT(bindrt <0);
		WAWO_ERR("[socket][%s]socket bind error, errno: %d", info().to_lencstr().cstr, bindrt);
	}

	void socket::ch_listen(WWRP<channel_promise> const& ch_promise, int const& backlog) {

		WWRP<socket> _this(this);
		if (!event_loop()->in_event_loop()) {
			event_loop()->schedule([_this, ch_promise, backlog]() ->void {
				_this->ch_listen(ch_promise, backlog);
			});
			return;
		}

		WAWO_ASSERT(m_state == S_BINDED);
		WAWO_ASSERT(fd() > 0);

		int rt = socket_base::listen(backlog);
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return;
		}

		if (!is_nonblocking()) {
			rt = turnon_nonblocking();
			if (rt != wawo::OK) {
				close();
			}
		}
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return;
		}

		m_state = S_LISTEN;
		begin_read(WATCH_OPTION_INFINITE, NULL, wawo::net::async_accept, wawo::net::async_error);
		WAWO_TRACE_SOCKET("[socket][%s]socket listen success", info().to_lencstr().cstr);
		ch_promise->set_success(rt);
	}

	u32_t socket::accept( WWRP<socket> sockets[], u32_t const& size, int& ec_o ) {

		if( m_state != S_LISTEN ) {
			ec_o = wawo::E_INVALID_STATE;
			return 0 ;
		}

		ec_o = wawo::OK;
		u32_t count = 0;

		do {
			address addr;
			int fd = socket_base::accept(addr);

			if( fd<0 ) {
				if ( WAWO_ABS(fd) == EINTR ) continue;
				if( !IS_ERRNO_EQUAL_WOULDBLOCK(WAWO_ABS(fd)) ) {
					ec_o = fd;
				}
				break;
			}

			WWRP<socket> so = wawo::make_ref<socket>(fd, addr, SM_PASSIVE, buffer_cfg(), sock_family(), sock_type(), sock_protocol(), OPTION_NONE);
			sockets[count++] = so;
		} while( count<size );

		if( count == size ) {
			ec_o=wawo::E_TRY_AGAIN;
		}

		return count ;
	}

	int socket::async_connect(address const& addr) {
		
		int rt = turnon_nonblocking();
		if (rt != wawo::OK) {
			return rt;
		}

		rt = connect(addr);
		if (rt == wawo::OK) {
			WWRP<channel> ch(this);
			wawo::task::fn_lambda _lambda = [ch]() {
				ch->ch_connected();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(_lambda);
			event_loop()->schedule(_t);
			return wawo::OK;
		} else if (rt == wawo::E_SOCKET_CONNECTING) {
			TRACE_IOE("[socket_base][%s][async_connect]watch(IOE_WRITE)", info().to_lencstr().cstr );
			begin_connect(NULL);
			return wawo::OK;
		}

		return rt;
	}

	void socket::handle_async_accept(int& ec_o) {

		do {
			WWRP<socket> accepted_sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
			u32_t count = accept(accepted_sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec_o ) ;

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

				channel::ch_accepted( accepted_sockets[i]);
				accepted_sockets[i]->ch_connected();
				accepted_sockets[i]->begin_read(WATCH_OPTION_INFINITE);
			}
		} while (ec_o == wawo::E_TRY_AGAIN);

		if (ec_o != wawo::OK) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
			ch_close(ch_promise);
		}
	}

	void socket::handle_async_read(int& ec_o ) {

		WAWO_ASSERT(event_loop()->in_event_loop());
		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( is_nonblocking() );

		ec_o = wawo::OK;
		do {
			WWRP<wawo::packet> arrives[10];
			u32_t count = _receive_packets(arrives, 10, ec_o);
			if (count > 0) {
				for (u32_t i = 0; i < count; i++) {
					channel::ch_read(arrives[i]);
				}
				if (!(m_rflag&WATCH_OPTION_INFINITE)) {
					channel::end_read();
				}
			}
		} while (ec_o == wawo::OK);
	}
}} //end of ns

#ifdef WAWO_PLATFORM_WIN
namespace wawo { namespace net {
	class WinsockInit : public wawo::singleton<WinsockInit> {
	public:
		WinsockInit() {
			WSADATA wsaData;
			int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			WAWO_CONDITION_CHECK(result == 0);
		}
		~WinsockInit() {
			WSACleanup();
		}
	};
	static WinsockInit const& __wawo_winsock_init__ = *(WinsockInit::instance());
}}
#endif