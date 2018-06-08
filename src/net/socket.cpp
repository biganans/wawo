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

	WWRP<channel_future> socket::async_bind(wawo::net::address const& address) {
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
		return async_bind(address, ch_promise);
	}

	WWRP<channel_future> socket::async_bind(wawo::net::address const& addr, WWRP<channel_promise>const& ch_promise) {
		WWRP<socket> _this(this);
		if (!event_loop()->in_event_loop()) {
			event_loop()->schedule([_this,addr,ch_promise]() ->void {
				_this->async_bind(addr, ch_promise);
			});
			return ch_promise;
		}

		WAWO_ASSERT(m_state == S_OPENED);

		int bindrt = socket_base::bind(addr);
		ch_promise->set_success(bindrt);
		if (bindrt == 0) {
			m_state = S_BINDED;
			WAWO_TRACE_SOCKET("[socket][%s]socket bind ok", info().to_lencstr().cstr );
			return ch_promise;
		}
		WAWO_ASSERT(bindrt <0);
		WAWO_ERR("[socket][%s]socket bind error, errno: %d", info().to_lencstr().cstr, bindrt);
		return ch_promise;
	}

	WWRP<channel_future> socket::async_listen(int const& backlog ) {
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
		return async_listen(ch_promise, backlog);
	}

	WWRP<channel_future> socket::async_listen(WWRP<channel_promise> const& ch_promise, int const& backlog ) {

		WWRP<socket> _this(this);
		if (!event_loop()->in_event_loop()) {
			event_loop()->schedule([_this, ch_promise, backlog]() ->void {
				_this->async_listen(ch_promise, backlog);
			});
			return ch_promise;
		}

		WAWO_ASSERT(m_state == S_BINDED);
		WAWO_ASSERT(fd() > 0);

		int rt = socket_base::listen(backlog);
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}

		if (!is_nonblocking()) {
			rt = turnon_nonblocking();
			if (rt != wawo::OK) {
				close();
			}
		}
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}

		m_state = S_LISTEN;
		fn_io_event _fn_accept = std::bind(&socket::__cb_async_accept, WWRP<socket>(this));
		fn_io_event_error _fn_err = std::bind(&socket::__cb_async_error, WWRP<socket>(this), std::placeholders::_1);

		begin_read(WATCH_OPTION_INFINITE, _fn_accept, _fn_err);
		WAWO_TRACE_SOCKET("[socket][%s]socket listen success", info().to_lencstr().cstr);
		ch_promise->set_success(rt);
		return ch_promise;
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

	WWRP<channel_future> socket::async_connect(address const& addr) {
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
		return async_connect(addr, ch_promise);
	}

	WWRP<channel_future> socket::async_connect(address const& addr, WWRP<channel_promise> const& ch_promise) {

		if (!event_loop()->in_event_loop()) {
			WWRP<socket> _so(this);
			event_loop()->schedule([_so,addr,ch_promise]() {
				_so->async_connect(addr,ch_promise);
			});
			return ch_promise;
		}

		int rt = turnon_nonblocking();
		if (rt != wawo::OK) {
			ch_promise->set_success(rt);
			return ch_promise;
		}

		rt = socket::connect(addr);
		if (rt == wawo::OK) {
			ch_promise->set_success(wawo::OK);
			channel::ch_connected();
			return ch_promise;
		} else if (rt == wawo::E_SOCKET_CONNECTING) {
			TRACE_IOE("[socket_base][%s][async_connect]watch(IOE_WRITE)", info().to_lencstr().cstr );
			ch_promise->set_success(wawo::OK);
			socket::begin_connect();
			return ch_promise;
		} else {
			ch_promise->set_success(rt);
			channel::ch_errno(rt);
			channel::ch_error();
			channel::ch_close();
			return ch_promise;
		}
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