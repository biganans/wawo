#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void socket::_init() {
		m_trb = (byte_t*) ::malloc( sizeof(byte_t)*m_cfg.buffer.rcv_size ) ;
		WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef _DEBUG
		::memset( m_trb, 'i', m_cfg.buffer.rcv_size );
#endif
	}

	void socket::_deinit() {
		WAWO_ASSERT( m_state == S_CLOSED);
		::free( m_trb );
		m_trb = NULL;
	}

	int socket::open( socket_cfg const& cfg ) {
		WAWO_ASSERT(m_state==S_CLOSED);
		int rt = socket_base::open(cfg);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt==wawo::OK);

		_init();
		channel::ch_fire_opened();
		m_state = S_OPENED;
		return rt;
	}

	int socket::connect(address const& addr) {
		if (!(m_state==S_OPENED || m_state==S_BINDED)) {
			return wawo::E_INVALID_STATE;
		}
		int rt= socket_base::connect(addr);
		if (rt == wawo::OK) {
			m_state = S_CONNECTED;
			return wawo::OK;
		}
		WAWO_ASSERT(rt== wawo::E_SOCKET_ERROR);
		WAWO_ASSERT(is_nonblocking());
		int ec = wawo::socket_get_last_errno();
		if (IS_ERRNO_EQUAL_CONNECTING(ec)) {
			m_state = S_CONNECTING;
		}
		return wawo::E_SOCKET_ERROR;
	}

	int socket::bind(wawo::net::address const& addr) {
		WAWO_ASSERT(event_poller()->in_event_loop());
		WAWO_ASSERT(m_state == S_OPENED);
		int rt = socket_base::bind(addr);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		m_state = S_BINDED;
		WAWO_TRACE_SOCKET("[socket][%s]socket bind ok", info().to_stdstring().c_str() );
		return wawo::OK;
	}

	int socket::listen(socket_cfg const& child_cfg, int const& backlog ) {
		WAWO_ASSERT(event_poller()->in_event_loop());

		WAWO_ASSERT(m_state == S_BINDED);
		WAWO_ASSERT(fd() > 0);

		int rt = socket_base::listen( child_cfg, backlog);
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

		m_state = S_LISTEN;
		WAWO_TRACE_SOCKET("[socket][%s]socket listen success", info().to_stdstring().c_str());
		return wawo::OK;
	}

	WWRP<channel_future> socket::_listen_on(address const& addr, fn_channel_initializer const& fn_accepted, socket_cfg const& cfg, socket_cfg const& child_cfg, int const& backlog) {
		WWRP<channel_promise> ch_p = make_promise();
		_listen_on(addr,fn_accepted,ch_p,cfg,child_cfg, backlog);
		return ch_p;
	}

	void socket::_listen_on(address const& addr, fn_channel_initializer const& fn_accepted, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg, socket_cfg const& child_cfg, int const& backlog ) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, addr, fn_accepted, ch_promise,cfg, child_cfg, backlog]() ->void {
				_this->_listen_on(addr, fn_accepted, ch_promise, cfg, child_cfg, backlog);
			});
			return;
		}
		int rt = socket::open(cfg);
		//int rt = -10043;
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			event_poller()->schedule([ch_promise,rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}
		rt = socket::bind(addr);
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::listen(child_cfg, backlog);
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			ch_promise->set_success(rt);
			return;
		}
		m_fn_accept_initializer = fn_accepted;
		WAWO_ASSERT(rt == wawo::OK);
#ifdef WAWO_IO_MODE_IOCP
		__IOCP_CALL_AcceptEx();
#else
		begin_accept();
#endif
	}

	WWRP<channel_future> socket::_dial(address const& addr, fn_channel_initializer const& initializer, socket_cfg const& cfg) {
		WWRP<channel_promise> f = make_promise();
		_dial(addr,initializer, f, cfg);
		return f;
	}

	void socket::_dial(address const& addr, fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, addr, initializer, ch_promise, cfg]() ->void {
				_this->_dial(addr, initializer, ch_promise, cfg);
			});
			return;
		}

		int rt = socket::open(cfg);
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = m_protocol == P_UDP ? _cfg_setup_udp(cfg) : _cfg_setup_tcp(cfg);
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::connect(addr);
		if (rt == wawo::OK) {
			WAWO_ASSERT(m_state == S_CONNECTED);
			initializer(WWRP<channel>(this));
			event_poller()->schedule([ch_promise, rt,SO=WWRP<socket>(this)]() {
				ch_promise->set_success(wawo::OK);
				SO->ch_fire_connected();
#ifdef WAWO_IO_MODE_IOCP
				SO->__IOCP_init();
#endif
				SO->begin_read();
			});
			return;
		} 
		if (rt == wawo::E_SOCKET_ERROR) {
			rt = wawo::socket_get_last_errno();
			if ( IS_ERRNO_EQUAL_CONNECTING(rt) ) {
				WAWO_ASSERT(m_state == S_CONNECTING);
				m_fn_dial_initializer = initializer;
				m_dial_promise = ch_promise;
				TRACE_IOE("[socket][%s][async_connect]watch(IOE_WRITE)", info().to_stdstring().c_str());
#ifdef WAWO_IO_MODE_IOCP
				socket::__IOCP_CALL_ConnectEx();
#else
				socket::begin_connect();
#endif
			} else {
				event_poller()->schedule([ch_promise, rt, CH=WWRP<channel>(this)]() {
					ch_promise->set_success(rt);
					CH->ch_errno(rt);
					CH->ch_close();
				});
			}
		}
	}

	SOCKET socket::accept( address& raddr ) {
		if (m_state != S_LISTEN) {
			wawo::set_last_errno(wawo::E_INVALID_STATE);
			return wawo::E_SOCKET_ERROR;
		}

		return socket_base::accept(raddr);
	}
}} //end of ns