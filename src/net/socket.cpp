#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void async_connected(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);

		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);

		WAWO_ASSERT(cookie->success != NULL);
		WAWO_ASSERT(cookie->error != NULL);
		WAWO_ASSERT(cookie->so != NULL);
		WAWO_ASSERT(cookie->so->is_active());
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);

		TRACE_IOE("[socket_base][#%d:%s][async_connected], unwatch(IOE_WRITE)", so->get_fd(), so->get_addr_info().cstr);
		so->end_async_write();
		so->handle_async_connected();
		WAWO_TRACE_SOCKET("[socket][#%d]socket async connected, local addr: %s, remote addr: %s", so->get_fd(), so->get_local_addr().address_info().cstr, so->get_remote_addr().address_info().cstr);

		WWRP<async_cookie> handshake_cookie = wawo::make_ref<async_cookie>();
		handshake_cookie->so = cookie->so;
		handshake_cookie->user_cookie = cookie->user_cookie;
		handshake_cookie->success = cookie->success;
		handshake_cookie->error = cookie->error;

		int rt = so->tlp_handshake(cookie->user_cookie, cookie->success, cookie->error);

		if (rt == wawo::E_TLP_HANDSHAKE_DONE) {
			cookie->success(cookie->user_cookie);
			return;
		}

		if (rt != wawo::E_TLP_HANDSHAKING) {
			cookie->error(rt, cookie->user_cookie);
			return;
		}
	}

	void async_connect_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);

		TRACE_IOE("[socket_base][%d:%s][async_connect_error]error code: %d, unwatch(IOE_WRITE)", so->get_fd(), so->get_addr_info().cstr, code);
		so->end_async_write();

		WAWO_ASSERT(cookie->success != NULL);
		WAWO_ASSERT(cookie->error != NULL);
		WAWO_ASSERT(cookie->so != NULL);
		WAWO_ASSERT(so->is_active());

		WAWO_TRACE_SOCKET("[socket][#%d]socket async connect error, local addr: %s, remote addr: %s", so->get_fd(), so->get_local_addr().address_info().cstr, so->get_remote_addr().address_info().cstr);
		cookie->error(code, cookie->user_cookie);
	}

	void async_handshake(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
		WAWO_ASSERT(so != NULL);

		int ec;
		so->handle_async_handshake(ec);

		switch (ec) {
		case wawo::E_TLP_HANDSHAKING:
			{}
			break;
		case wawo::OK:
		case wawo::E_SOCKET_RECV_BLOCK:
		case wawo::E_SOCKET_PUMP_TRY_AGAIN:
		case wawo::E_TLP_TRY_AGAIN:
			{
				WAWO_ASSERT(so->get_tlp()->handshake_done());
			}
			break;
		case wawo::E_TLP_HANDSHAKE_DONE:
			{
				so->end_async_read();
				WAWO_ASSERT(cookie->success != NULL);
				WAWO_ASSERT(cookie->user_cookie != NULL);
				cookie->success(cookie->user_cookie);
				WAWO_TRACE_SOCKET("[socket][#%d:%s]async_handshake ok", so->get_fd(), so->get_addr_info().cstr);
			}
			break;
		default:
			{
				so->end_async_read();
				cookie->error(ec, cookie->user_cookie);
				WAWO_TRACE_SOCKET("[socket][#%d:%s]async_handshake, tlp error: %d", so->get_fd(), so->get_addr_info().cstr, ec);
			}
			break;
		}
	}

	void async_handshake_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
		WAWO_ASSERT(so != NULL);

		WAWO_ASSERT(cookie->error != NULL);
		WAWO_ASSERT(cookie->user_cookie != NULL);
		cookie->error(code, cookie->user_cookie);
		WAWO_TRACE_SOCKET("[socket][#%d:%s]async_handshake error: %d", so->get_fd(), so->get_addr_info().cstr, code );
	}

	void async_accept(WWRP<ref_base> const& cookie_) {
	
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
		int ec;

		so->handle_async_accept(ec);
	}


	void async_read(WWRP<ref_base> const& cookie_) {

		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
		int ec;

		so->handle_async_read(ec);
		switch (ec) {
			case wawo::OK:
			case wawo::E_SOCKET_RECV_BLOCK:
			case wawo::E_TLP_HANDSHAKE_DONE:
			case wawo::E_TLP_HANDSHAKING:
			{}
			break;
			case wawo::E_SOCKET_GRACE_CLOSE:
			{
				so->shutdown(SHUTDOWN_RD);
			}
			break;
			case wawo::E_SOCKET_RD_SHUTDOWN_ALREADY:
			{
			}
			break;
			case wawo::E_SOCKET_RECV_BUFFER_FULL:
			{
				WAWO_THROW("socket logic issue, recv buffer full should not be prompted to outside")
			}
			break;
			default:
			{
				WAWO_TRACE_SOCKET("[socket][#%d:%s]async read, pump error: %d, close", so->get_fd(), so->get_addr_info().cstr, ec);
				so->close(ec);
			}
		}
	}

	void async_write(WWRP<ref_base> const& cookie_) {

		/*
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
		int ec;
		so->handle_async_write(ec);

		switch (ec) {
			case wawo::E_SOCKET_SEND_BLOCK:
			case wawo::OK:
			{
			}
			break;
			case wawo::E_SOCKET_WR_SHUTDOWN_ALREADY:
			{
				WAWO_WARN("[socket][#%d:%s]async send error: %d", so->get_fd(), so->get_addr_info().cstr, ec);
			}
			break;
			case wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED:
			default:
			{
				WAWO_WARN("[socket][#%d:%s]async send error: %d", so->get_fd(), so->get_addr_info().cstr, ec);
				int recv_ec;
				do {
					WWSP<wawo::packet> arrives[5];
					u32_t count = so->receive_packets(arrives, 5, recv_ec, F_RCV_ALWAYS_PUMP);
					for (u32_t i = 0; i < count; ++i) {
						WWRP<socket_event> sevt = wawo::make_ref<socket_event>(E_PACKET_ARRIVE, so, arrives[i]);
						so->trigger(sevt);
					}
				} while (recv_ec == wawo::E_SOCKET_PUMP_TRY_AGAIN);

				WAWO_WARN("[socket][#%d:%s]async send error: %d, close", so->get_fd(), so->get_addr_info().cstr, ec);
				so->close(ec);
			}
			break;
		}
		*/
	}

	void async_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);

		WAWO_ASSERT(so != NULL);

		WAWO_WARN("[socket][#%d:%s]socket error: %d, close", so->get_fd(), so->get_addr_info().cstr, code);
		so->close(code);
	}
}}

namespace wawo { namespace net {

	void socket::_init() {
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );

		m_tsb = (byte_t*) ::malloc( sizeof(byte_t)*m_sbc.snd_size ) ;
		WAWO_CONDITION_CHECK( m_tsb != NULL );

#ifdef _DEBUG
		::memset( m_tsb, 'i', m_sbc.snd_size );
#endif
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		m_trb = (byte_t*) ::malloc( sizeof(byte_t)*m_sbc.rcv_size ) ;
		WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef _DEBUG
		::memset( m_trb, 'i', m_sbc.rcv_size );
#endif
		WAWO_ASSERT( m_rb == NULL );
		m_rb = wawo::make_ref<wawo::bytes_ringbuffer>( m_sbc.rcv_size ) ;
		WAWO_ALLOC_CHECK( m_rb, sizeof(wawo::bytes_ringbuffer) );

		WAWO_ASSERT( m_sb == NULL );
		m_sb = wawo::make_ref<wawo::bytes_ringbuffer>( m_sbc.snd_size ) ;
		WAWO_ALLOC_CHECK( m_sb, sizeof(wawo::bytes_ringbuffer));

		WAWO_ASSERT(m_rps_q == NULL);
		WAWO_ASSERT(m_rps_q_standby == NULL);

		m_rps_q = new std::queue<WWSP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q, sizeof(std::queue<WWSP<wawo::packet>>) );

		m_rps_q_standby = new std::queue<WWSP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q_standby, sizeof(std::queue<WWSP<wawo::packet>>));
	}

	void socket::_deinit() {
		WAWO_ASSERT( m_state == S_CLOSED) ;

		::free( m_tsb );
		m_tsb = NULL;

		::free( m_trb );
		m_trb = NULL;

		WAWO_ASSERT( m_rb != NULL );
		m_rb = NULL;
		WAWO_ASSERT( m_sb != NULL );
		m_sb =NULL;

		WAWO_ASSERT(m_rps_q != NULL);
		WAWO_ASSERT(m_rps_q->size() == 0);
		WAWO_DELETE(m_rps_q);

		WAWO_ASSERT(m_rps_q_standby != NULL);
		WAWO_ASSERT(m_rps_q_standby->size() == 0);
		WAWO_DELETE(m_rps_q_standby);
	}

	int socket::close( int const& ec ) {

		lock_guard<spin_mutex> lg_s(m_mutexes[L_SOCKET]);
		if (m_state == S_CLOSED) {
			return wawo::E_INVALID_STATE;
		}

		if( is_nonblocking() &&
			is_data_socket() &&
			!is_write_shutdowned() )
		{
			u32_t left;
			int _ec;
			flush(left,_ec, ec==wawo::OK?__FLUSH_BLOCK_TIME_INFINITE__: __FLUSH_DEFAULT_BLOCK_TIME__);
		}
		
		u8_t sflag = 0;
		{
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			if (!(m_rflag&SHUTDOWN_RD)) {
				m_rflag |= SHUTDOWN_RD;
				sflag |= SHUTDOWN_RD;
			}
			if (is_nonblocking()) {
				_end_async_read();
			}
		}

		{
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			if (!(m_wflag&SHUTDOWN_WR)) {
				m_wflag |= SHUTDOWN_WR;
				sflag |= SHUTDOWN_WR;
			}
			if (is_nonblocking()) {
				_end_async_write();
			}
		}
		

		int old_state = m_state;
		int closert = _close( ec );
		if (!is_nonblocking()) return closert;

		if (is_listener()) {
			//fire close

			//WWRP<socket_event> evt_close = wawo::make_ref<socket_event>(E_CLOSE, WWRP<socket>(this), udata64(ec));
			//_dispatcher_t::oschedule(evt_close);
			//return closert;
		}

		if (old_state != S_CONNECTED) {
			WAWO_ASSERT(ec !=0 );
			
			//fire error

			//WWRP<socket_event> evt_error = wawo::make_ref<socket_event>(E_ERROR, WWRP<socket>(this), udata64(ec));
			//_dispatcher_t::oschedule(evt_error);
			//return closert;
		}

		/*
		 * fire accordingly
		WWRP<socket> so(this);		
		_dispatcher_t::fn_lambda_t lambda_FNR = [so, sflag, ec]() -> void {
			if (sflag&SHUTDOWN_RD) {
				WWRP<socket_event> evt_rd = wawo::make_ref<socket_event>(E_RD_SHUTDOWN, so, udata64(ec));
				so->trigger(evt_rd);
			}
			if (sflag&SHUTDOWN_WR) {
				WWRP<socket_event> evt_wr = wawo::make_ref<socket_event>(E_WR_SHUTDOWN, so, udata64(ec));
				so->trigger(evt_wr);
			}
			WWRP<socket_event> evt_close = wawo::make_ref<socket_event>(E_CLOSE, so, udata64(ec));
			so->trigger(evt_close);
		};
		_dispatcher_t::oschedule(lambda_FNR);
		*/

		return closert;
	}

	int socket::shutdown(u8_t const& flag, int const& ec ) {
		lock_guard<spin_mutex> lg_s(m_mutexes[L_SOCKET]);

		WAWO_ASSERT(flag == SHUTDOWN_RD ||
			flag == SHUTDOWN_WR ||
			flag == SHUTDOWN_RDWR
		);

		if( is_nonblocking() &&
			is_data_socket() &&
			!is_write_shutdowned()
			)
		{
			u32_t left;
			int _ec;
			flush(left,_ec);
		}

		u8_t sflag;
		int shutrt = socket_base::_shutdown(flag, sflag );
		WAWO_TRACE_SOCKET("[socket][#%d:%s]shutdown(%u) for(%u), sflag:%u, shutrt: %d", m_fd, m_addr.address_info().cstr, flag, ec, sflag, shutrt);

		if( sflag == 0 ) {
			WAWO_ASSERT(shutrt == wawo::E_INVALID_OPERATION);
			WAWO_TRACE_SOCKET("[socket][#%d:%s]shutdown(%u) for(%d), ignore", m_fd, m_addr.address_info().cstr, flag, ec );
			return wawo::E_INVALID_OPERATION;
		}

		if (!is_nonblocking()) return shutrt;
		WAWO_ASSERT(sflag != 0);

		/*
			fire accordingly
		
		WWRP<socket> so(this);
		_dispatcher_t::fn_lambda_t lambda_FNR = [so, sflag, ec ]() -> void {
			if (sflag&SHUTDOWN_RD) {
				WWRP<socket_event> evt_rd = wawo::make_ref<socket_event>(E_RD_SHUTDOWN, so, udata64(ec));
				so->trigger(evt_rd);
			}
			if (sflag&SHUTDOWN_WR) {
				WWRP<socket_event> evt_wr = wawo::make_ref<socket_event>(E_WR_SHUTDOWN, so, udata64(ec));
				so->trigger(evt_wr);
			}
			so->__rdwr_check(ec);
		};

		_dispatcher_t::oschedule(lambda_FNR);
		*/

		return shutrt;
	}

	int socket::listen(int const& backlog, fn_socket_init const& fn_init, WWRP<ref_base> const& cookie) {
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);

		WAWO_ASSERT(m_sm == SM_NONE);
		WAWO_ASSERT(m_state == S_BINDED);
		WAWO_ASSERT(m_fd>0);

		int listenrt;

		if (m_protocol == P_UDP) {
			listenrt = wawo::OK;
		}
		else {
			listenrt = m_fn_listen(m_fd, backlog);
		}

		WAWO_RETURN_V_IF_NOT_MATCH(listenrt, listenrt == wawo::OK);

		m_sm = SM_LISTENER;
		m_state = S_LISTEN;

		if (fn_init != NULL) {
			if (!is_nonblocking()) {
				int trt = turnon_nonblocking();
				if (trt != wawo::OK) {
					close();
					return trt;
				}
			}

			m_fn_socket_init = fn_init;
			m_init_cookie = cookie;
			begin_async_read(WATCH_OPTION_INFINITE, cookie, wawo::net::async_accept, wawo::net::async_error);
		}

		WAWO_TRACE_SOCKET("[socket][#%d:%s]socket listen success, protocol: %s", m_fd, m_addr.address_info().cstr, protocol_str[m_protocol]);
		return wawo::OK;
	}

	u32_t socket::accept( WWRP<socket> sockets[], u32_t const& size, int& ec_o ) {

		WAWO_ASSERT( m_sm == SM_LISTENER );

		if( m_state != S_LISTEN ) {
			ec_o = wawo::E_INVALID_STATE;
			return 0 ;
		}

		ec_o = wawo::OK;
		u32_t count = 0;
		sockaddr_in addr_in;
		socklen_t addr_length = sizeof(addr_in);

		lock_guard<spin_mutex> lg( m_mutexes[L_READ] );

		do {
			address addr;
			int fd = m_fn_accept(m_fd, reinterpret_cast<sockaddr*>(&addr_in), &addr_length );

			if( fd<0 ) {
				if ( WAWO_ABS(fd) == EINTR ) continue;
				if( !IS_ERRNO_EQUAL_WOULDBLOCK(WAWO_ABS(fd)) ) {
					ec_o = fd;
				}
				break;
			}

			addr.set_netsequence_port( (addr_in.sin_port) );
			addr.set_netsequence_ulongip( (addr_in.sin_addr.s_addr) );

			WWRP<socket> so = wawo::make_ref<socket>(fd, addr, SM_PASSIVE, m_sbc, m_family, m_type, m_protocol, OPTION_NONE);
			sockets[count++] = so;
		} while( count<size );

		if( count == size ) {
			ec_o=wawo::E_TRY_AGAIN;
		}

		return count ;
	}

	u32_t socket::send( byte_t const* const buffer, u32_t const& size, int& ec_o, int const& flag ) {

		int sent = 0;
		/*
		WAWO_ASSERT( buffer != NULL );
		WAWO_ASSERT( size > 0 );
		WAWO_ASSERT( size <= m_sb->capacity() );

		if( m_wflag&SHUTDOWN_WR ) {
			ec_o = wawo::E_SOCKET_WR_SHUTDOWN_ALREADY;
			return 0;
		}

		if (size > m_sbc.snd_size) {
			ec_o = wawo::E_SOCKET_LARGE_PACKET;
			return 0;
		}

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE]);
		//if (is_nonblocking() && (m_sb->count()>0) ) {
		//	ec_o = wawo::E_SOCKET_SEND_BLOCK;
		//	return 0;
		//}
		
		if( is_nonblocking() ) {
			WAWO_ASSERT( m_sb != NULL );
			if( m_sb->count()>0 ) {
				if ((flag&F_SEND_USE_SND_BUFFER) && m_sb->left_capacity()>size) {
					u32_t wcount = m_sb->write((byte_t*)(buffer), size);
					WAWO_ASSERT(wcount == size);
					WAWO_TRACE_SOCKET("[socket][#%d:%s]socket::send(): write to sb: %d",
						m_fd, m_addr.address_info().cstr, wcount);
					return wcount;
				}
				ec_o = wawo::E_SOCKET_SEND_BLOCK;
				return 0;
			}
		}

		u32_t sent = socket_base::send(buffer, size, ec_o, flag&(~F_SEND_USE_SND_BUFFER));
		if( (sent<size) && ec_o == wawo::E_SOCKET_SEND_BLOCK ) {

			WAWO_ASSERT( is_nonblocking() );
			WAWO_ASSERT( m_sb != NULL );

			u32_t to_buffer_c = size-sent ;

			WAWO_ASSERT( m_sb->left_capacity() >= to_buffer_c ) ;

			wawo::u32_t write_c = m_sb->write( (byte_t*)(buffer) + sent, to_buffer_c ) ;
			WAWO_ASSERT( to_buffer_c == write_c );

			WAWO_TRACE_SOCKET("[socket][#%d:%s]socket::send() blocked (write to sb: %d), sent: %d" , m_fd, m_addr.address_info().cstr, write_c, sent ) ;
			sent += write_c;

			m_async_wt = wawo::time::curr_milliseconds();

			WWRP<socket_event> evt = wawo::make_ref<socket_event>(E_WR_BLOCK, WWRP<socket>(this), udata64(1) );
			_dispatcher_t::oschedule( evt );

			_begin_async_write(); //only write once
		}
		*/
		return sent;
	}

	u32_t socket::recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag ) {
		lock_guard<spin_mutex> lg( m_mutexes[L_READ]);
		return socket_base::recv( buffer_o, size, ec_o, flag );
	}

	void socket::_pump(int& ec_o, int const& flag ) {

		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( m_rb != NULL );
		ec_o = wawo::OK;

		u32_t left_space = m_rb->left_capacity() ;
		if( 0 == left_space ) {
			WAWO_ERR("[socket][#%d:%s]_pump m_rb->LeftSpace() == 0, skip read, please retry", m_fd, m_addr.address_info().cstr);
			ec_o = wawo::E_SOCKET_RECV_BUFFER_FULL ;
			return ;
		}

		u32_t can_trb_recv_s = ( m_sbc.rcv_size <= left_space) ?  m_sbc.rcv_size : left_space ;
		u32_t recv_BC = socket_base::recv( m_trb, can_trb_recv_s, ec_o,flag );
		if (recv_BC > 0) {
			wawo::u32_t rb_WBC = m_rb->write(m_trb, recv_BC);
			WAWO_ASSERT(recv_BC == rb_WBC);
		}

		WAWO_TRACE_SOCKET("[socket][#%d:%s]pump bytes: %d, ec: %d", m_fd, m_addr.address_info().cstr , recv_BC, ec_o );
	}

	void socket::handle_async_accept(int& ec_o) {

		do {

			WWRP<socket> accepted_sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
			u32_t count = accept(accepted_sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec_o ) ;

			for (u32_t i = 0; i < count; ++i) {
				WWRP<socket>& so = accepted_sockets[i];

				int nonblocking = so->turnon_nonblocking();
				if (nonblocking != wawo::OK) {
					WAWO_ERR("[node_abstract][#%d:%s]turn on nonblocking failed: %d", so->get_fd(), so->get_addr_info().cstr, nonblocking);
					accepted_sockets[i]->close(nonblocking);
					continue;
				}

				int setdefaultkal = so->set_keep_alive_vals(default_keep_alive_vals);
				if (setdefaultkal != wawo::OK) {
					WAWO_ERR("[node_abstract][#%d:%s]set_keep_alive_vals failed: %d", so->get_fd(), so->get_addr_info().cstr, setdefaultkal);
					accepted_sockets[i]->close(nonblocking);
					continue;
				}

				WAWO_ASSERT(m_fn_socket_init != NULL);
				WAWO_ASSERT(m_init_cookie != NULL);

				m_fn_socket_init(accepted_sockets[i], m_init_cookie);
			}
		} while (ec_o == wawo::E_TRY_AGAIN);

		if (ec_o != wawo::OK) {
			close(ec_o);
		}
	}

	void socket::handle_async_handshake(int& ec_o) {
		WAWO_ASSERT(is_data_socket());
		WAWO_ASSERT(m_rb != NULL);
		WAWO_ASSERT(is_nonblocking());

		ec_o = wawo::OK;
		lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
		if (m_tlp->handshake_done()) {
			ec_o = wawo::OK;
			return;
		}
		int flag = F_RCV_ALWAYS_PUMP;
		do {
			WWSP<wawo::packet> arrives[1];
			u32_t count = _receive_packets(arrives, 0, ec_o, flag);
			WAWO_ASSERT(count == 0);
			flag = (ec_o == wawo::E_SOCKET_PUMP_TRY_AGAIN) ? F_RCV_ALWAYS_PUMP : 0;
		} while ( (ec_o == wawo::E_TLP_TRY_AGAIN || ec_o == wawo::E_SOCKET_PUMP_TRY_AGAIN) && m_tlp->handshake_handshaking() );

		if (m_tlp->handshake_done() && 
			(ec_o == wawo::OK || ec_o == wawo::E_TLP_TRY_AGAIN || ec_o == wawo::E_SOCKET_PUMP_TRY_AGAIN || ec_o == wawo::E_SOCKET_RECV_BLOCK)
			)
		{
			ec_o = wawo::E_TLP_HANDSHAKE_DONE;
		}
	}

	void socket::handle_async_read(int& ec_o ) {

		//read from fd
		//pass msg to pipe for read evt

		WAWO_ASSERT(!"todo");
		/*

		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( m_rb != NULL );
		WAWO_ASSERT( is_nonblocking() );

		bool has_new_arrives = false;
		{
			ec_o = wawo::OK;
			lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
			bool is_one_time_async_read = !(m_wflag&WATCH_OPTION_INFINITE);
			if (is_one_time_async_read) {
				_end_async_read();
			}

			int flag = F_RCV_ALWAYS_PUMP;
			do {
				WWSP<wawo::packet> arrives[5];
				u32_t count = _receive_packets(arrives, 5, ec_o, flag);
				flag = (ec_o == wawo::E_SOCKET_PUMP_TRY_AGAIN) ? F_RCV_ALWAYS_PUMP : 0;
				if (count > 0) {
					lock_guard<spin_mutex> lg_rq_standby(m_rps_q_standby_mutex);
					has_new_arrives = true;
					for (u32_t i = 0; i < count; i++) {
						m_rps_q_standby->push(arrives[i]);
					}
				}
			} while (ec_o == wawo::E_TLP_TRY_AGAIN || ec_o == wawo::E_SOCKET_PUMP_TRY_AGAIN);

			if (is_one_time_async_read && !has_new_arrives) {
				_begin_async_read();
			}
		}

		if (has_new_arrives) {

			lock_guard<spin_mutex> lg_rq(m_rps_q_mutex);
			while (m_rps_q->size()) {
				wawo::this_thread::yield();
			}
			WAWO_ASSERT(m_rps_q->size() == 0);
			{
				lock_guard<spin_mutex> lg_rq_standby(m_rps_q_standby_mutex);
				if (m_rps_q_standby->size()) {
					std::swap(m_rps_q, m_rps_q_standby);
					WAWO_ASSERT(m_rps_q_standby->size() == 0);
					WAWO_ASSERT(m_rps_q->size() != 0);
				}
			}

			while (m_rps_q->size()) {
				WWSP<packet>& p = m_rps_q->front();
				WWRP<socket_event> evt = wawo::make_ref<socket_event>(E_PACKET_ARRIVE, WWRP<socket>(this), p);
				_dispatcher_t::trigger(evt);
				m_rps_q->pop();
			}
		}
		*/
	}

	void socket::flush(u32_t& left, int& ec_o, int const& block_time /* in microseconds*/) {

		if( !is_nonblocking() ) { ec_o = wawo::E_INVALID_OPERATION; return ; }

		WAWO_ASSERT( m_sb != NULL );
		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u64_t begin_time = 0 ;
		u32_t k = 0;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		do {
			flushed_total += _flush( left, ec_o );
			if( (left == 0) || block_time == 0 || (ec_o != wawo::E_SOCKET_SEND_BLOCK) ) {
				break;
			}

			u64_t now = wawo::time::curr_microseconds();
			if(begin_time==0) {
				begin_time = now;
			}

			if( (block_time != -1) && (begin_time-now)>((u32_t)block_time) ) {
				break;
			}
			++k;
			wawo::this_thread::usleep(WAWO_MIN(__WAIT_FACTOR__*k, __FLUSH_SLEEP_TIME_MAX__) );
		} while( true );

		if (left == 0) { m_async_wt = 0; }

		WAWO_TRACE_SOCKET( "[socket][#%d:%s]flush end, sent: %d, left: %d", m_fd, m_addr.address_info().cstr, flushed_total, left );
	}

	void socket::handle_async_write( int& ec_o) {

		WAWO_ASSERT(!"TODO");

		/*
		WAWO_ASSERT( is_nonblocking() );
		WAWO_ASSERT( m_sb != NULL );
		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u32_t left;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		if ( !(m_wflag&WATCH_OPTION_INFINITE)) {
			_end_async_write();
		}

		do {
			flushed_total += _flush( left, ec_o );
			if( (left == 0) || (ec_o != wawo::OK) ) {
				break;
			}
		} while( true );

		if( left == 0 ) {
			WAWO_ASSERT( ec_o == wawo::OK );
			if (flushed_total != 0) {
				_end_async_write();
				m_async_wt = 0;
				WWRP<socket_event> evt = wawo::make_ref<socket_event>(E_WR_UNBLOCK, WWRP<socket>(this), udata64(0) );
				_dispatcher_t::oschedule(evt);
			}
		} else {
			WAWO_ASSERT(m_async_wt != 0);
			if( ec_o == wawo::E_SOCKET_SEND_BLOCK) {
				u64_t now = wawo::time::curr_milliseconds() ;

				if( (flushed_total == 0) && ( now > (m_async_wt+m_delay_wp)) ) {
					ec_o = wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED;
				} else {
					if( (flushed_total>0) ) {
						m_async_wt = now; //update timer
					}
					_begin_async_write(); //only write once
				}
			}
		}
		WAWO_TRACE_SOCKET( "[socket][#%d:%s]handle_async_write, flushed_total: %d, left: %d", m_fd, m_addr.address_info().cstr, flushed_total, left );
		*/
	}

	u32_t socket::_flush( u32_t& left, int& ec_o ) {
		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( is_nonblocking() );

		u32_t flushed = 0;
		left = m_sb->count();
		ec_o = wawo::OK;

		while( (left!=0) && (ec_o == wawo::OK) ) {
			u32_t to_send = m_sb->peek( m_tsb, m_sbc.snd_size );
			u32_t sent = socket_base::send( m_tsb, to_send, ec_o,0) ;
			flushed += sent ;

			if( sent > 0 ) {
				m_sb->skip(sent);
			}
			left = m_sb->count() ;
		}

		return flushed;
	}

	int socket::send_packet(WWSP<wawo::packet> const& packet, int const& flag ) {
		WAWO_ASSERT( packet != NULL);
		WAWO_ASSERT( m_tlp != NULL );
		WWSP<wawo::packet> TLP_Pack;

		int rt = m_tlp->encode(packet, TLP_Pack);
		if (rt != wawo::OK) {
			return rt;
		}

		int send_ec;
		return (socket::send(TLP_Pack->begin(), TLP_Pack->len(), send_ec, flag) == TLP_Pack->len()) ? wawo::OK : send_ec;
	}

	u32_t socket::receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o, int const& flag ) {
		lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
		return _receive_packets(arrives,size,ec_o, flag);
	}

	//return one packet at least
	u32_t socket::_receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o, int const& flag ) {
		bool force = (m_rb->count() ==0) ;

	force_pump:
		int pump_ec = wawo::OK ;
		if (force || (flag&F_RCV_ALWAYS_PUMP)) {
			_pump(pump_ec, (flag&(~(F_RCV_BLOCK_UNTIL_PACKET_ARRIVE|F_RCV_ALWAYS_PUMP))));

			if (!(
				(pump_ec == wawo::OK) ||
				(pump_ec == wawo::E_SOCKET_RECV_BLOCK) ||
				(pump_ec == wawo::E_SOCKET_RECV_BUFFER_FULL)
				)) {
				
				ec_o = pump_ec;
				return 0;
			}

			if (m_rb->count() == 0) {
				WAWO_ASSERT(is_nonblocking() ? pump_ec == wawo::E_SOCKET_RECV_BLOCK : true);
				WAWO_ASSERT( (!is_nonblocking()) ? pump_ec != wawo::OK : true );
				ec_o = pump_ec;
				return 0;
			}
		}

		WAWO_ASSERT(m_tlp != NULL);
		WWSP<wawo::packet> out;
		int tlp_state ;
		u32_t tlp_packet_count = m_tlp->decode_packets(m_rb, arrives, size, tlp_state, out);
		//WAWO_TRACE_SOCKET("[socket]new tlp packet arrived: %d, tlp_state: %d", tlp_packet_count, tlp_state );

		bool tlp_ok = (tlp_state == wawo::OK || tlp_state == wawo::E_TLP_HANDSHAKING || tlp_state == wawo::E_TLP_HANDSHAKE_DONE || tlp_state == wawo::E_TLP_TRY_AGAIN );

		if (!tlp_ok) {
			ec_o = tlp_state;
			return tlp_packet_count;
		}
		ec_o = tlp_state;

		if (out != NULL && out->len()) {
			int sndec;
			u32_t snd_bytes = send(out->begin(), out->len(), sndec);
			(void)&snd_bytes;
			WAWO_ASSERT(sndec == wawo::OK ? snd_bytes == out->len() : true);
			ec_o = (sndec != wawo::OK) ? sndec : ec_o;
		}

		if ((flag&F_RCV_BLOCK_UNTIL_PACKET_ARRIVE)&&(tlp_packet_count ==0)&&(size>0) ) {
			force = true;
			goto force_pump;
		}

		if ( (pump_ec == wawo::OK || pump_ec == wawo::E_SOCKET_RECV_BUFFER_FULL) && is_nonblocking() ) {
			ec_o = wawo::E_SOCKET_PUMP_TRY_AGAIN;
		}

		if (tlp_packet_count == size && m_rb->count()) {
			ec_o = wawo::E_TLP_TRY_AGAIN;
		}

		return tlp_packet_count;
	}

	int socket::tlp_handshake( WWRP<ref_base> const& cookie, fn_io_event const& success , fn_io_event_error const& error ) {
		WAWO_ASSERT(m_tlp != NULL);
		WWSP<wawo::packet> hello;

		//int ec;
		int handshake_state = m_tlp->handshake_make_hello_packet(hello);
		if (handshake_state == wawo::E_TLP_HANDSHAKE_DONE) {
			return wawo::E_TLP_HANDSHAKE_DONE;
		}

		int sndrt = socket_base::send(hello->begin(), hello->len(), handshake_state );
		(void)&sndrt;
		WAWO_RETURN_V_IF_NOT_MATCH(handshake_state, handshake_state == wawo::OK);

		if ( cookie != NULL && success != NULL && error != NULL) {
			//async
			begin_async_handshake( cookie, success, error);
			return wawo::E_TLP_HANDSHAKING;
		}

		while (m_tlp->handshake_handshaking()) {
			WWSP<wawo::packet> arrives[1];
			u32_t count = receive_packets(arrives, 0, handshake_state);
			WAWO_ASSERT(count == 0);

			if (handshake_state != wawo::E_TLP_TRY_AGAIN &&
				handshake_state != wawo::E_TLP_HANDSHAKING &&
				handshake_state != wawo::OK &&
				handshake_state != wawo::E_SOCKET_PUMP_TRY_AGAIN &&
				handshake_state != wawo::E_SOCKET_RECV_BLOCK)
			{
				break;
			}

			if (is_nonblocking()) {
				wawo::this_thread::sleep(5);
			}
			(void)count;
		}

		if (m_tlp->handshake_done()) {
			return wawo::E_TLP_HANDSHAKE_DONE;
		}

		return handshake_state;
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
