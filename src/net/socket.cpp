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
		/*
		so->handle_async_write(ec);

		switch (ec) {
			case wawo::E_CHANNEL_WRITE_BLOCK:
			case wawo::OK:
			{
			}
			break;
			case wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY:
			{
				WAWO_WARN("[socket][%s]async send error: %d", so->info().to_lencstr().cstr, ec);
			}
			break;
			case wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED:
			default:
			{
				so->close();
			}
			break;
		}
		*/
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

		WAWO_ASSERT(m_rps_q == NULL);
		WAWO_ASSERT(m_rps_q_standby == NULL);

		m_rps_q = new std::queue<WWRP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q, sizeof(std::queue<WWRP<wawo::packet>>) );

		m_rps_q_standby = new std::queue<WWRP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q_standby, sizeof(std::queue<WWRP<wawo::packet>>));

		m_outs = wawo::make_shared< std::queue<WWRP<wawo::packet>> >();

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

		WAWO_ASSERT(m_rps_q != NULL);
		WAWO_ASSERT(m_rps_q->size() == 0);
		WAWO_DELETE(m_rps_q);

		WAWO_ASSERT(m_rps_q_standby != NULL);
		WAWO_ASSERT(m_rps_q_standby->size() == 0);
		WAWO_DELETE(m_rps_q_standby);

		m_outs = NULL;
	}

	int socket::open() {
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
		WAWO_ASSERT(m_state == S_CLOSED);
		channel::ch_opened();
		int rt = socket_base::open();
		WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
		m_state = S_OPENED;
		{
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			m_rflag = 0;
		}

		{
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			m_wflag = 0;
		}

		return rt;
	}

	int socket::connect(address const& addr)
	{
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
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

	/*
	int socket::close() {

		lock_guard<spin_mutex> lg_s(m_mutexes[L_SOCKET]);
		if (m_state == S_CLOSED) {
			return wawo::E_CHANNEL_INVALID_STATE;
		}

		if( is_nonblocking() &&
			is_data_socket() &&
			!is_write_shutdowned() )
		{
			bool left;
			int _ec;
			flush(left,_ec);
		}
		
		u8_t sflag = 0;
		{
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			if (!(m_rflag&SHUTDOWN_RD)) {
				m_rflag |= SHUTDOWN_RD;
				sflag |= SHUTDOWN_RD;
			}
			if (is_nonblocking()) {
				_end_read();
			}
		}

		{
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			if (!(m_wflag&SHUTDOWN_WR)) {
				m_wflag |= SHUTDOWN_WR;
				sflag |= SHUTDOWN_WR;
			}
			if (is_nonblocking()) {
				_end_write();
			}
		}

		int old_state = m_state;
		int closert = socket_base::close();
		m_state = S_CLOSED;
		if (!is_nonblocking()) return closert;

		if (is_listener()) {
			//WWRP<channel_pipeline> pipeline(pipeline());
			WWRP<channel> ch(this);

			wawo::task::fn_lambda lambda_FNR = [ch]() -> void {
				ch->ch_closed();
				ch->deinit();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
			event_loop()->schedule(_t);
			return closert;
		}

		if (old_state != S_CONNECTED) {
			WWRP<channel> ch(this);

			wawo::task::fn_lambda lambda_FNR = [ch]() -> void {
				ch->ch_error();
				ch->ch_closed();
				ch->deinit();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
			event_loop()->schedule(_t);
			return closert;
		}
		
		{
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			m_rflag |= SHUTDOWN_RD;
		}

		{
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			m_wflag |= SHUTDOWN_WR;
		}

		WWRP<channel> ch(this);
		wawo::task::fn_lambda lambda_FNR = [ch, sflag]() -> void {
			if (sflag&SHUTDOWN_RD) {
				ch->ch_read_shutdowned();
			}

			if (sflag&SHUTDOWN_WR) {
				ch->ch_write_shutdowned();
			}

			ch->ch_closed();
			ch->deinit();
		};

		WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
		event_loop()->schedule(_t);

		return closert;
	}

	int socket::shutdown(u8_t const& flag ) {
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
			bool left;
			int _ec;
			flush(left,_ec);
		}

		u8_t sflag = 0;
		{
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			if ((flag&SHUTDOWN_RD) && !(m_rflag&SHUTDOWN_RD)) {
				m_rflag |= SHUTDOWN_RD;
				sflag |= SHUTDOWN_RD;
			}
		}

		{
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			if ((flag&SHUTDOWN_WR) && !(m_wflag&SHUTDOWN_WR)) {
				m_wflag |= SHUTDOWN_WR;
				sflag |= SHUTDOWN_WR;
			}
		}

		if (sflag == 0) {
			WAWO_TRACE_SOCKET("[socket][%s]shutdown(%u) for(%d), ignore", info().to_lencstr().cstr, flag, ec);
			return wawo::E_INVALID_OPERATION;
		}

		int _flag = 0;
		if (sflag == SHUTDOWN_RD) {
			_flag = SHUT_RD;
		}
		else if (sflag == SHUTDOWN_WR) {
			_flag = SHUT_WR;
		}
		else if (sflag == SHUTDOWN_RDWR) {
			_flag = SHUT_RDWR;
		}

		int shutrt = socket_base::shutdown(_flag);
		WAWO_TRACE_SOCKET("[socket][%s]shutdown(%u) for(%u), sflag:%u, shutrt: %d", info().to_lencstr().cstr, flag, ec, sflag, shutrt);

		if (!is_nonblocking()) return shutrt;
		WAWO_ASSERT(sflag != 0);

		WWRP<socket> so(this);
		wawo::task::fn_lambda lambda_FNR = [so, sflag]() -> void {
			if (sflag&SHUTDOWN_RD) {
				so->end_read();
				so->ch_read_shutdowned();
			}

			if (sflag&SHUTDOWN_WR) {
				so->end_write();
				so->ch_write_shutdowned();
			}
			so->__rdwr_check();
		};

		WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
		event_loop()->schedule(_t);

		return shutrt;
	}
	*/

	void socket::ch_bind(wawo::net::address const& addr, WWRP<channel_promise>const& ch_promise) {

		WWRP<socket> _this(this);
		if (!event_loop()->in_event_loop()) {
			event_loop()->schedule([_this,addr,ch_promise]() ->void {
				_this->ch_bind(addr, ch_promise);
			});
			return;
		}

		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
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

		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
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

		lock_guard<spin_mutex> lg( m_mutexes[L_READ] );

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
			close();
		}
	}

	void socket::handle_async_read(int& ec_o ) {

		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( is_nonblocking() );

		bool has_new_arrives = false;
		{
			ec_o = wawo::OK;
			lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
			bool is_one_time_async_read = !(m_rflag&WATCH_OPTION_INFINITE);
			if (is_one_time_async_read) {
				_end_read();
			}

			do {
				WWRP<wawo::packet> arrives[5];
				u32_t count = _receive_packets(arrives, 5, ec_o);
				if (count > 0) {
					lock_guard<spin_mutex> lg_rq_standby(m_rps_q_standby_mutex);
					has_new_arrives = true;
					for (u32_t i = 0; i < count; i++) {
						m_rps_q_standby->push(arrives[i]);
					}
				}
			} while (ec_o == wawo::OK);

			if (is_one_time_async_read && !has_new_arrives) {
				_begin_read();
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
				WWRP<packet>& p = m_rps_q->front();
				ch_read(p);
				m_rps_q->pop();
			}
		}
		
	}

	/*
	void socket::flush(bool& left, int& ec_o, int const& block_time ) // in microseconds
	{

		if( !is_nonblocking() ) { ec_o = wawo::E_INVALID_OPERATION; return ; }

		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u64_t begin_time = 0 ;
		u32_t k = 0;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		do {
			flushed_total += _flush( left, ec_o );
			if( (!left) || block_time == 0 || (ec_o != wawo::E_CHANNEL_WRITE_BLOCK) ) {
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

		WAWO_TRACE_SOCKET( "[socket][%s]flush end, sent: %d, left: %d", info().to_lencstr().cstr, flushed_total, left );
	}
	*/
	void socket::handle_async_write( int& ec_o) {
		WAWO_ASSERT( is_nonblocking() );
		socket::ch_flush_impl();

		/*
		ec_o = wawo::OK ;
		u32_t flushed_total = 0;
		bool left;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		if ( !(m_wflag&WATCH_OPTION_INFINITE)) {
			_end_write();
		}

		do {
			flushed_total += _flush( left, ec_o );
			if( (!left) || (ec_o != wawo::OK) ) {
				break;
			}
		} while( true );

		if( !left ) {
			WAWO_ASSERT( ec_o == wawo::OK );
			if (flushed_total != 0) {
				_end_write();
				m_async_wt = 0;

				WWRP<channel> ch(this);
				wawo::task::fn_lambda lambda = [ch]() -> void {
					ch->ch_write_unblock();
				};
				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				event_loop()->schedule(_t);
			}
		} else {
			WAWO_ASSERT(m_async_wt != 0);
			if( ec_o == wawo::E_CHANNEL_WRITE_BLOCK) {
				u64_t now = wawo::time::curr_milliseconds() ;

				if( (flushed_total == 0) && ( now > (m_async_wt+m_delay_wp)) ) {
					ec_o = wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED;
				} else {
					if( (flushed_total>0) ) {
						m_async_wt = now; //update timer
					}
					_begin_write(); //only write once
				}
			}
		}
		WAWO_TRACE_SOCKET( "[socket][%s]handle_async_write, flushed_total: %d, left: %d", info().to_lencstr().cstr, flushed_total, left );
		*/
	}

	/*
	u32_t socket::_flush( bool& left, int& ec_o ) {

		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( is_nonblocking() );

		u32_t flushed = 0;
		ec_o = wawo::OK;

		left = m_outs->size()>0;
		while( (left) && (ec_o == wawo::OK) ) {
			WWRP<wawo::packet>& outp = m_outs->front();
			WAWO_ASSERT(outp->len() > 0);

			u32_t sbytes = socket_base::send(outp->begin(), outp->len(), ec_o) ;

			if (sbytes > 0) {
				WAWO_TRACE_SOCKET("[socket]sent: %u, left: %u", sbytes, outp->len() );

				outp->skip(sbytes);
				flushed += sbytes;

				if (outp->len() == 0) {
					WAWO_TRACE_SOCKET("[socket]pop one outp from queue");
					m_outs->pop();
				}
			}
			left = m_outs->size() > 0;
		}

		return flushed;
	}

	int socket::send_packet(WWRP<wawo::packet> const& outp, int const& flag ) {
		WAWO_ASSERT(outp != NULL);
		WAWO_ASSERT(outp->len() > 0);

		bool is_block = false;
		int send_ec;
		{
			lock_guard<spin_mutex> lw(m_mutexes[L_WRITE]);
			if (m_outs->size() > 0) {
				if (m_outs->size() > 10) {
					return wawo::E_CHANNEL_WRITE_BLOCK;
				}

				WAWO_TRACE_SOCKET("[socket]push one outp for queue not empty");
				//we have to make our own copy
				WWRP<packet> _outp = wawo::make_ref<packet>(outp->begin(), outp->len() );
				m_outs->push(_outp);
				return wawo::OK;
			}

			u32_t sbytes = socket_base::send(outp->begin(), outp->len(), send_ec, flag);
			WAWO_ASSERT(sbytes >= 0);
			outp->skip(sbytes);

			if (outp->len() > 0) {
				WWRP<packet> _outp = wawo::make_ref<packet>( outp->begin(), outp->len());
				m_outs->push(_outp);
				WAWO_TRACE_SOCKET("[socket][%s]push one outp to queue for socket::send() blocked, left: %u, sent: %u", info().to_lencstr().cstr, _outp->len(), sbytes);
			}

			if ( send_ec == wawo::E_CHANNEL_WRITE_BLOCK) {
				m_async_wt = wawo::time::curr_milliseconds();
				is_block = true;
				send_ec = wawo::OK;
			}
		}

		if (is_block) {
			ch_write_block();
			begin_write(); //only write once
		}

		return send_ec;
	}
	*/

	u32_t socket::receive_packets(WWRP<wawo::packet> arrives[], u32_t const& size, int& ec_o ) {
		lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
		return _receive_packets(arrives,size,ec_o);
	}

	u32_t socket::_receive_packets(WWRP<wawo::packet> arrives[], u32_t const& size, int& ec_o ) {
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