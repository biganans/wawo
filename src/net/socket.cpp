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

		TRACE_IOE("[socket_base][%s][async_connected], unwatch(IOE_WRITE)", so->info().to_lencstr().cstr );
		so->end_async_write();
		WAWO_TRACE_SOCKET("[socket][%s]socket async connected", so->info().to_lencstr().cstr );
		so->handle_async_connected();
	}

	void async_connect_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);

		TRACE_IOE("[socket_base][%s][async_connect_error]error code: %d, unwatch(IOE_WRITE)", so->info().to_lencstr().cstr, code);
		so->end_async_write();

		WAWO_ASSERT(cookie->success != NULL);
		WAWO_ASSERT(cookie->error != NULL);
		WAWO_ASSERT(cookie->so != NULL);
		WAWO_ASSERT(so->is_active());

		WAWO_TRACE_SOCKET("[socket][%s]socket async connect error", so->info().to_lencstr().cstr);
		so->handle_async_connect_error(code);
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
				WAWO_TRACE_SOCKET("[socket][%s]async read, pump error: %d, close", so->info().to_lencstr().cstr, ec);
				so->close(ec);
			}
		}
	}

	void async_write(WWRP<ref_base> const& cookie_) {
		
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
				WAWO_WARN("[socket][%s]async send error: %d", so->info().to_lencstr().cstr, ec);
			}
			break;
			case wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED:
			default:
			{
				so->close(ec);
			}
			break;
		}
	}

	void async_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);

		WAWO_ASSERT(so != NULL);

		WAWO_WARN("[socket][%s]socket error: %d, close", so->info().to_lencstr().cstr, code);
		so->close(code);
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

		m_rps_q = new std::queue<WWSP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q, sizeof(std::queue<WWSP<wawo::packet>>) );

		m_rps_q_standby = new std::queue<WWSP<wawo::packet>>;
		WAWO_ALLOC_CHECK(m_rps_q_standby, sizeof(std::queue<WWSP<wawo::packet>>));

		m_outs = wawo::make_shared< std::queue<WWSP<wawo::packet>> >();

		init_pipeline();
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

	void socket::init_pipeline()
	{
		WAWO_ASSERT(m_observer == NULL);
		m_observer = wawo::net::observers::instance()->next();

		m_pipeline = wawo::make_ref<socket_pipeline>(WWRP<socket>(this));
		m_pipeline->init();
		WAWO_ALLOC_CHECK(m_pipeline, sizeof(socket_pipeline));
	}

	void socket::deinit_pipeline()
	{
		m_observer = NULL;
		if (m_pipeline != NULL) {
			m_pipeline->deinit();
			m_pipeline = NULL;
		}
	}

	int socket::open() {
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
		WAWO_ASSERT(m_state == S_CLOSED);

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

	int socket::close( int const& ec ) {

		lock_guard<spin_mutex> lg_s(m_mutexes[L_SOCKET]);
		if (m_state == S_CLOSED) {
			return wawo::E_INVALID_STATE;
		}

		if( is_nonblocking() &&
			is_data_socket() &&
			!is_write_shutdowned() )
		{
			bool left;
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
		int closert = socket_base::close();
		m_ec = ec;
		m_state = S_CLOSED;
		if (!is_nonblocking()) return closert;

		if (is_listener()) {
			WWRP<socket_pipeline> pipeline(m_pipeline);
			WWRP<socket> so(this);

			wawo::task::fn_lambda lambda_FNR = [pipeline,so]() -> void {
				pipeline->fire_closed();
				so->deinit_pipeline();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
			m_observer->plan(_t);
			return closert;
		}

		if (old_state != S_CONNECTED) {
			WAWO_ASSERT(ec !=0 );
			
			WWRP<socket_pipeline> pipeline(m_pipeline);
			WWRP<socket> so(this);

			wawo::task::fn_lambda lambda_FNR = [pipeline,so,ec]() -> void {
				pipeline->fire_error();
				pipeline->fire_closed();
				so->deinit_pipeline();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
			m_observer->plan(_t);
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

		WWRP<socket_pipeline> pipeline(m_pipeline);
		WWRP<socket> so(this);
		wawo::task::fn_lambda lambda_FNR = [pipeline, so, sflag]() -> void {
			if (sflag&SHUTDOWN_RD) {
				pipeline->fire_read_shutdowned();
			}

			if (sflag&SHUTDOWN_WR) {
				pipeline->fire_write_shutdowned();
			}

			pipeline->fire_closed();
			so->deinit_pipeline ();
		};

		WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
		m_observer->plan(_t);

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

		m_ec = ec;
		int shutrt = socket_base::shutdown(_flag);
		WAWO_TRACE_SOCKET("[socket][%s]shutdown(%u) for(%u), sflag:%u, shutrt: %d", info().to_lencstr().cstr, flag, ec, sflag, shutrt);

		if (!is_nonblocking()) return shutrt;
		WAWO_ASSERT(sflag != 0);

		WWRP<socket_pipeline> pipeline(m_pipeline);
		WWRP<socket> so(this);
		wawo::task::fn_lambda lambda_FNR = [pipeline, so, sflag,ec]() -> void {
			if (sflag&SHUTDOWN_RD) {
				so->end_async_read();
				pipeline->fire_read_shutdowned();
			}

			if (sflag&SHUTDOWN_WR) {
				so->end_async_write();
				pipeline->fire_write_shutdowned();
			}
			so->__rdwr_check(ec);
		};

		WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda_FNR);
		m_observer->plan(_t);

		return shutrt;
	}

	int socket::bind(wawo::net::address const& addr) {
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
		WAWO_ASSERT(m_state == S_OPENED);

		int bindrt = socket_base::bind(addr);

		if (bindrt == 0) {
			m_state = S_BINDED;
			WAWO_TRACE_SOCKET("[socket][%s]socket bind ok", info().to_lencstr().cstr );
			return wawo::OK;
		}
		WAWO_ASSERT(bindrt <0);
		WAWO_ERR("[socket][%s]socket bind error, errno: %d", info().to_lencstr().cstr, bindrt);
		return bindrt;
	}

	int socket::listen(int const& backlog) {
		lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);

		WAWO_ASSERT(m_state == S_BINDED);

		int listenrt = socket_base::listen(backlog);

		WAWO_RETURN_V_IF_NOT_MATCH(listenrt, listenrt == wawo::OK);
		m_state = S_LISTEN;

		if (!is_nonblocking()) {
			int trt = turnon_nonblocking();
			if (trt != wawo::OK) {
				close();
				return trt;
			}
		}

		begin_async_read(WATCH_OPTION_INFINITE, NULL, wawo::net::async_accept, wawo::net::async_error);
		WAWO_TRACE_SOCKET("[socket][%s]socket listen success", info().to_lencstr().cstr);
		return wawo::OK;
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
		
		WAWO_ASSERT(m_pipeline != NULL);

		int rt = turnon_nonblocking();
		if (rt != wawo::OK) {
			return rt;
		}

		rt = connect(addr);
		if (rt == wawo::OK) {
			WWRP<socket> so(this);
			wawo::task::fn_lambda _lambda = [&so]() {
				so->pipeline()->fire_connected();
			};

			WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(_lambda);
			m_observer->plan(_t);
			return wawo::OK;
		} else if (rt == wawo::E_SOCKET_CONNECTING) {
			TRACE_IOE("[socket_base][%s][async_connect]watch(IOE_WRITE)", info().to_lencstr().cstr );
			begin_async_connect(NULL);
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
					accepted_sockets[i]->close(nonblocking);
					continue;
				}

				int setdefaultkal = so->set_keep_alive_vals(default_keep_alive_vals);
				if (setdefaultkal != wawo::OK) {
					WAWO_ERR("[node_abstract][%s]set_keep_alive_vals failed: %d", so->info().to_lencstr().cstr, setdefaultkal);
					accepted_sockets[i]->close(nonblocking);
					continue;
				}

				WAWO_ASSERT(m_pipeline != NULL);

				m_pipeline->fire_accepted( accepted_sockets[i]);
				accepted_sockets[i]->pipeline()->fire_connected();
				accepted_sockets[i]->begin_async_read(WATCH_OPTION_INFINITE);
			}
		} while (ec_o == wawo::E_TRY_AGAIN);

		if (ec_o != wawo::OK) {
			close(ec_o);
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
				_end_async_read();
			}

			do {
				WWSP<wawo::packet> arrives[5];
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
				m_pipeline->fire_read(p);
				m_rps_q->pop();
			}
		}
		
	}

	void socket::flush(bool& left, int& ec_o, int const& block_time /* in microseconds*/) {

		if( !is_nonblocking() ) { ec_o = wawo::E_INVALID_OPERATION; return ; }

		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u64_t begin_time = 0 ;
		u32_t k = 0;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		do {
			flushed_total += _flush( left, ec_o );
			if( (!left) || block_time == 0 || (ec_o != wawo::E_SOCKET_SEND_BLOCK) ) {
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

	void socket::handle_async_write( int& ec_o) {
		WAWO_ASSERT( is_nonblocking() );

		ec_o = wawo::OK ;
		u32_t flushed_total = 0;
		bool left;

		lock_guard<spin_mutex> lg( m_mutexes[L_WRITE] );
		if ( !(m_wflag&WATCH_OPTION_INFINITE)) {
			_end_async_write();
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
				_end_async_write();
				m_async_wt = 0;

				WWRP<socket> so(this);
				wawo::task::fn_lambda lambda = [&so]() -> void {
					so->pipeline()->fire_write_unblock();
				};
				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				m_observer->plan(_t);
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
		WAWO_TRACE_SOCKET( "[socket][%s]handle_async_write, flushed_total: %d, left: %d", info().to_lencstr().cstr, flushed_total, left );
	}

	u32_t socket::_flush( bool& left, int& ec_o ) {

		WAWO_ASSERT( is_data_socket() );
		WAWO_ASSERT( is_nonblocking() );

		u32_t flushed = 0;
		ec_o = wawo::OK;

		left = m_outs->size()>0;
		while( (left) && (ec_o == wawo::OK) ) {
			WWSP<wawo::packet>& outp = m_outs->front();
			WAWO_ASSERT(outp->len() > 0);

			u32_t sbytes = socket_base::send(outp->begin(), outp->len(), ec_o) ;

			if (sbytes > 0) {
				outp->skip(sbytes);
				flushed += sbytes;
			}

			if (outp->len() == 0) {
				m_outs->pop();
			}
			left = m_outs->size() > 0;
		}

		return flushed;
	}

	int socket::send_packet(WWSP<wawo::packet> const& packet, int const& flag ) {
		WAWO_ASSERT( packet != NULL);
		
		bool is_block = false;
		int send_ec;
		{
			lock_guard<spin_mutex> lw(m_mutexes[L_WRITE]);
			if (m_outs->size() > 0) {
				m_outs->push(packet);
				return wawo::OK;
			}

			u32_t sbytes = socket_base::send(packet->begin(), packet->len(), send_ec, flag);
			WAWO_ASSERT(sbytes >= 0);

			if (sbytes < packet->len() && send_ec == wawo::E_SOCKET_SEND_BLOCK) {
				packet->skip(sbytes);
				m_outs->push(packet);
				WAWO_TRACE_SOCKET("[socket][%s]socket::send() blocked, left: %u, sent: %u", info().to_lencstr().cstr, packet->len(), sbytes);
				m_async_wt = wawo::time::curr_milliseconds();
				is_block = true;

				send_ec = wawo::OK;
			}
		}

		if (is_block) {
			m_pipeline->fire_write_block();
			begin_async_write(); //only write once
		}

		return send_ec;
	}

	u32_t socket::receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o ) {
		lock_guard<spin_mutex> lg(m_mutexes[L_READ]);
		return _receive_packets(arrives,size,ec_o);
	}

	u32_t socket::_receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o ) {
		u32_t n = 0;
		do {
			u32_t nbytes = socket_base::recv(m_trb, buffer_cfg().rcv_size, ec_o);
			if (nbytes > 0) {
				WWSP<packet> p = wawo::make_shared<packet>(nbytes);
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