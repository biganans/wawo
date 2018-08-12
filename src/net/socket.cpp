#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void socket::_init() {
		WAWO_ASSERT(m_cfg.buffer.rcv_size > 0);
		m_trb = (byte_t*) ::malloc( sizeof(byte_t)*m_cfg.buffer.rcv_size ) ;
		WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef ENABLE_DEBUG_MEMORY_ALLOC
		::memset( m_trb, 's', m_cfg.buffer.rcv_size );
#endif
	}

	void socket::_deinit() {
		WAWO_ASSERT( m_state == S_CLOSED);
		::free( m_trb );
		m_trb = NULL;
	}

	int socket::open() {
		WAWO_ASSERT(m_state==S_CLOSED);
		int rt = socket_base::open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		channel::ch_fire_open();
		m_state = S_OPENED;
		return wawo::OK;
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

		WAWO_ASSERT(is_nonblocking());
		if (IS_ERRNO_EQUAL_CONNECTING(rt)) {
			m_state = S_CONNECTING;
		}
		return rt;
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

	WWRP<channel_future> socket::listen_on(address const& addr, fn_channel_initializer const& fn_accepted, socket_cfg const& cfg, socket_cfg const& child_cfg, int const& backlog) {
		WWRP<channel_promise> ch_p = make_promise();
		listen_on(addr,fn_accepted,ch_p,cfg,child_cfg, backlog);
		return ch_p;
	}

	void socket::listen_on(address const& addr, fn_channel_initializer const& fn_accepted, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg, socket_cfg const& child_cfg, int const& backlog ) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, addr, fn_accepted, ch_promise,cfg, child_cfg, backlog]() ->void {
				_this->listen_on(addr, fn_accepted, ch_promise, cfg, child_cfg, backlog);
			});
			return;
		}
		int rt = socket::open();
		if (rt != wawo::OK) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::init(cfg);
		if (rt != wawo::OK) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		//int rt = -10043;
		rt = socket::bind(addr);
		if (rt != wawo::OK) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::listen(child_cfg, backlog);
		if (rt != wawo::OK) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		WAWO_ASSERT(rt == wawo::OK);
		m_fn_accept_initializer = fn_accepted;

		event_poller()->schedule([ch_promise, S=WWRP<socket>(this)]() {
			ch_promise->set_success(wawo::OK);
#ifdef WAWO_IO_MODE_IOCP
			S->__IOCP_CALL_AcceptEx();
#else
			S->begin_accept();
#endif
	});

	}

	WWRP<channel_future> socket::dial(address const& addr, fn_channel_initializer const& initializer, socket_cfg const& cfg) {
		WWRP<channel_promise> f = make_promise();
		dial(addr,initializer,f,cfg);
		return f;
	}

	void socket::dial(address const& addr, fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise, socket_cfg const& cfg) {
		if (!event_poller()->in_event_loop()) {
			WWRP<socket> _this(this);
			event_poller()->execute([_this, addr, initializer, ch_promise, cfg]() ->void {
				_this->dial(addr, initializer, ch_promise, cfg);
			});
			return;
		}

		int rt = socket::open();
		if (rt != wawo::OK ) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::init(cfg);
		if (rt != wawo::OK) {
			event_poller()->schedule([ch_promise, rt]() {
				ch_promise->set_success(rt);
			});
			return;
		}

		rt = socket::connect(addr);
		if (rt == wawo::OK) {
			WAWO_ASSERT(m_state == S_CONNECTED);
			initializer(WWRP<channel>(this));
			event_poller()->schedule([ch_promise, rt,so=WWRP<socket>(this)]() {
				ch_promise->set_success(wawo::OK);
				so->ch_fire_connected();
#ifdef WAWO_IO_MODE_IOCP
				so->__IOCP_init();
#endif
				so->begin_read();
			});
			return;
		}

		if ( IS_ERRNO_EQUAL_CONNECTING(rt) ) {
			WAWO_ASSERT(m_state == S_CONNECTING);
			m_fn_dial_initializer = initializer;
			m_dial_promise = ch_promise;
			WAWO_TRACE_IOE("[socket][%s][async_connect]watch(IOE_WRITE)", info().to_stdstring().c_str());
#ifdef WAWO_IO_MODE_IOCP
			socket::__IOCP_CALL_ConnectEx();
#else
			socket::begin_connect();
#endif
		} else {
			//error
			event_poller()->schedule([ch_promise, rt, CH = WWRP<channel>(this)]() {
				ch_promise->set_success(rt);
			});
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