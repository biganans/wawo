#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void socket::_init() {
		m_trb = (byte_t*) ::malloc( sizeof(byte_t)*buffer_cfg().rcv_size ) ;
		WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef _DEBUG
		::memset( m_trb, 'i', buffer_cfg().rcv_size );
#endif
	}

	void socket::_deinit() {
		WAWO_ASSERT( m_state == S_CLOSED);
		::free( m_trb );
		m_trb = NULL;
	}

	int socket::open() {
		WAWO_ASSERT(m_state == S_CLOSED);
		int rt = socket_base::open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		channel::ch_fire_opened();
		m_state = S_OPENED;
		m_rflag = 0;
		m_wflag = 0;
		return rt;
	}

	int socket::connect(address const& addr) {
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

	int socket::bind(wawo::net::address const& addr) {
		WAWO_ASSERT(event_poller()->in_event_loop());
		WAWO_ASSERT(m_state == S_OPENED);
		int rt = socket_base::bind(addr);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		m_state = S_BINDED;
		WAWO_TRACE_SOCKET("[socket][%s]socket bind ok", info().to_lencstr().cstr );
		return wawo::OK;
	}

	int socket::listen( int const& backlog ) {
		WAWO_ASSERT(event_poller()->in_event_loop());

		WAWO_ASSERT(m_state == S_BINDED);
		WAWO_ASSERT(fd() > 0);

		int rt = socket_base::listen(backlog);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

		m_state = S_LISTEN;
		WAWO_TRACE_SOCKET("[socket][%s]socket listen success", info().to_lencstr().cstr);
		return wawo::OK;
	}

	WWRP<wawo::net::channel_future> socket::_listen_on(address const& addr, fn_accepted_channel_initializer const& fn_accepted, int const& backlog) {
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
		return socket::_listen_on(addr, fn_accepted, ch_promise, backlog);
	}

	WWRP<wawo::net::channel_future> socket::_listen_on(address const& addr, fn_accepted_channel_initializer const& fn_accepted, WWRP<channel_promise> const& ch_promise, int const& backlog ) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, addr, fn_accepted, backlog, ch_promise]() ->void {
				_this->_listen_on(addr, fn_accepted, ch_promise, backlog);
			});
			return ch_promise;
		}
		int rt = socket::open();
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}
		rt = turnon_nonblocking();
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}

		rt = socket::bind(addr);
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}
		rt = socket::listen(backlog);
		ch_promise->set_success(rt);

#ifdef WAWO_ENABLE_IOCP
		begin_listen();
#endif

		if (rt == wawo::OK) {
			fn_io_event _fn_accept = std::bind(&socket::__cb_async_accept, WWRP<socket>(this), std::placeholders::_1);
			fn_io_event_error _fn_err = std::bind(&socket::__cb_async_error, WWRP<socket>(this), std::placeholders::_1, std::placeholders::_2);
			begin_read(WATCH_OPTION_INFINITE, _fn_accept, _fn_err);
		}

		m_fn_accepted = fn_accepted;
		return ch_promise;
	}

	WWRP<channel_future> socket::_dial(address const& addr, fn_dial_channel_initializer const& initializer) {
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
		return _dial(addr, initializer, ch_promise);
	}

	WWRP<channel_future> socket::_dial(address const& addr, fn_dial_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, initializer, addr, ch_promise]() ->void {
				_this->_dial(addr, initializer, ch_promise);
			});
			return ch_promise;
		}

		int rt = socket::open();
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}
		rt = turnon_nonblocking();
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}

		initializer( WWRP<channel>(this) );

		rt = socket::connect(addr);
		if (rt == wawo::OK) {
			ch_promise->set_success(wawo::OK);
			channel::ch_fire_connected();
			return ch_promise;
		} else if (rt == wawo::E_SOCKET_CONNECTING) {
			TRACE_IOE("[socket_base][%s][async_connect]watch(IOE_WRITE)", info().to_lencstr().cstr);
			ch_promise->set_success(wawo::OK);
			socket::begin_connect();
			return ch_promise;
		} else {
			ch_promise->set_success(rt);
			channel::ch_errno(rt);
			channel::ch_fire_error();
			channel::ch_close();
			return ch_promise;
		}
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

			int fire_open_ok = true;
			try {
				so->ch_fire_opened();
			} catch (...) {
				fire_open_ok = false;
			}
			if (fire_open_ok) {
				sockets[count++] = so;
			} else {
				so->close();
			}
		} while( count<size );

		if( count == size ) {
			ec_o=wawo::E_TRY_AGAIN;
		}

		return count ;
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