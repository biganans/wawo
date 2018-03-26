#ifndef _WAWO_NET_SOCKET_HPP_
#define _WAWO_NET_SOCKET_HPP_

#include <queue>

#include <wawo/smart_ptr.hpp>
#include <wawo/time/time.hpp>
#include <wawo/thread/mutex.hpp>

#include <wawo/packet.hpp>
#include <wawo/net/address.hpp>

#include <wawo/net/socket_base.hpp>
#include <wawo/net/socket_pipeline.hpp>

#include <wawo/net/socket_observer.hpp>

#include <future>

#define WAWO_MAX_ASYNC_WRITE_PERIOD	(90000L) //90 seconds

namespace wawo { namespace net {

	class socket;
	typedef void (*fn_socket_init)(WWRP<socket> const& so, WWRP<ref_base> const& cookie);

	struct async_cookie :
		public ref_base
	{
		WWRP<socket_base> so;
		WWRP<ref_base> cookie;
		fn_io_event success;
		fn_io_event_error error;
	};

	void async_connected(WWRP<ref_base> const& cookie_);
	void async_connect_error(int const& code, WWRP<ref_base> const& cookie_);

	void async_accept(WWRP<ref_base> const& so_);
	void async_read(WWRP<ref_base> const& so_);
	void async_write(WWRP<ref_base> const& so_);
	void async_error(int const& code, WWRP<ref_base> const& so_);

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

	enum LockType {
		L_SOCKET = 0,
		L_READ,
		L_WRITE,
		L_MAX
	};

	class socket:
		public socket_base
	{
		spin_mutex m_mutexes[L_MAX];
		u8_t	m_state;
		u8_t	m_rflag;
		u8_t	m_wflag;

		WWRP<ref_base>	m_ctx;

		byte_t* m_trb; //tmp read buffer

		u64_t m_delay_wp;
		u64_t m_async_wt;

		spin_mutex m_rps_q_mutex;
		spin_mutex m_rps_q_standby_mutex;

		std::queue<WWSP<wawo::packet>> *m_rps_q;
		std::queue<WWSP<wawo::packet>> *m_rps_q_standby;

		WWSP<std::queue<WWSP<wawo::packet>>> m_outs;

		WWRP<socket_pipeline> m_pipeline;

		int m_ec;
		void _init();
		void _deinit();

	public:
		explicit socket(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc ,family const& family , type const& sockt, protocol const& proto, option const& opt = OPTION_NONE ):
			socket_base(fd,addr,sm,sbc,family, sockt,proto,opt),
			m_state(S_CONNECTED),
			m_rflag(0),
			m_wflag(0),
			m_ctx(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL),
			m_pipeline(NULL),
			m_ec(0)
		{
			_init();
		}

		explicit socket(family const& family, type const& type, protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(family, type, proto, opt),
			m_state(S_CLOSED),
			m_rflag(0),
			m_wflag(0),
			m_ctx(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL),
			m_pipeline(NULL),
			m_ec(0)
		{
			_init();
		}

		explicit socket(socket_buffer_cfg const& sbc, family const& family,type const& type_, protocol const& proto, option const& opt = OPTION_NONE) :
			socket_base(sbc, family, type_, proto, opt),
			m_state(S_CLOSED),
			m_rflag(0),
			m_wflag(0),
			m_ctx(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL),
			m_pipeline(NULL),
			m_ec(0)
		{
			_init();
		}

		~socket() 
		{
			_deinit();
		}

		inline bool is_connected() const { return m_state == S_CONNECTED; }
		inline bool is_connecting() const { return (m_state == S_CONNECTING); }
		inline bool is_closed() const { return (m_state == S_CLOSED); }

		inline bool is_read_shutdowned() const { return (m_rflag&SHUTDOWN_RD) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&SHUTDOWN_WR) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag | m_wflag)&SHUTDOWN_RDWR) == SHUTDOWN_RDWR); }

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			lock_guard<spin_mutex> _lg(*(const_cast<spin_mutex*>(&m_mutexes[L_SOCKET])));
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
			m_ctx = ctx;
		}

		int get_ec() const { return m_ec; }

		//@TODO: timer to add
		inline bool is_flush_timer_expired(u64_t const& now /*in milliseconds*/) {
			if (m_async_wt == 0) return false;
			lock_guard<spin_mutex> lg(m_mutexes[L_WRITE]);
			return ((m_outs->size()>0) && (m_async_wt != 0) && (now>(m_async_wt + m_delay_wp)));
		}

		WWRP<socket_pipeline> pipeline() 
		{
			return m_pipeline;
		}

		int open();
		int connect(address const& addr);

		int close(int const& ec=0);
		int shutdown(u8_t const& flag, int const& ec=0);

		int bind(wawo::net::address const& address);
		int listen(int const& backlog = 128);
		u32_t accept( WWRP<socket> sockets[], u32_t const& size, int& ec_o ) ;

		int async_connect(address const& addr);

		int send_packet(WWSP<wawo::packet> const& packet, int const& flags = 0);
		u32_t receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o);

		void flush(bool& left, int& ec_o, int const& block_time = __FLUSH_DEFAULT_BLOCK_TIME__ /* in microseconds , -1 == INFINITE */ ) ;

		inline void handle_async_connected() {
			{
				lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
				WAWO_ASSERT(is_nonblocking());
				WAWO_ASSERT(m_state == S_CONNECTING);
				m_state = S_CONNECTED;
			}
			m_pipeline->fire_connected();
			begin_async_read(WATCH_OPTION_INFINITE);
		}

		inline void handle_async_connect_error(int const& code) {
			{
				lock_guard<spin_mutex> _lg(m_mutexes[L_SOCKET]);
				WAWO_ASSERT(is_nonblocking());
				WAWO_ASSERT(m_state == S_CONNECTING);
				WAWO_ASSERT(m_pipeline != NULL);
			}
			close(code);
		}

		void handle_async_accept(int& ec_o);
		void handle_async_read(int& ec_o);
		void handle_async_write( int& ec_o);

	private:
		
		void init_pipeline();
		void deinit_pipeline();

		u32_t _receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o );
		u32_t _flush(bool& left, int& ec_o);

		inline void __rdwr_check(int const& ec = 0) {
			u8_t nflag = 0;
			{
				lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
				if (m_rflag& SHUTDOWN_RD) {
					nflag |= SHUTDOWN_RD;
				}
			}
			{
				lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
				if (m_wflag& SHUTDOWN_WR) {
					nflag |= SHUTDOWN_WR;
				}
			}
			if (nflag == SHUTDOWN_RDWR) {
				close(ec);
			}
		}

		inline void _end_async_read() {
			if ((m_rflag&WATCH_READ) && is_nonblocking()) {
				m_rflag &= ~(WATCH_READ | WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_async_read]unwatch IOE_READ", info().to_lencstr()().cstr);
				unwatch(is_wcp(), IOE_READ, fd() );
			}
#ifdef _DEBUG
			else {
				TRACE_IOE("[socket][%s][end_async_read]unwatch IOE_READ, no op", info().to_lencstr().cstr );
			}
#endif
		}

		inline void _end_async_write() {
			if ((m_wflag&WATCH_WRITE) && is_nonblocking()) {
				m_wflag &= ~(WATCH_WRITE | WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_async_write]unwatch IOE_WRITE", info().to_lencstr().cstr );
				unwatch(is_wcp(), IOE_WRITE, fd() );
			}
#ifdef _DEBUG
			else {
				TRACE_IOE("[socket][%s][end_async_write]unwatch IOE_WRITE, no op", info().to_lencstr().cstr );
			}
#endif
		}

	public:
		inline void begin_async_connect( WWRP<ref_base> const& cookie, fn_io_event const& fn = wawo::net::async_connected, fn_io_event_error const& err = wawo::net::async_connect_error) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);

			WAWO_ASSERT(!(m_wflag&SHUTDOWN_WR));
			WAWO_ASSERT(!(m_wflag&WATCH_WRITE));
			m_wflag |= WATCH_WRITE;
			TRACE_IOE("[socket][%s][begin_async_connect]watch IOE_WRITE", info().to_lencstr().cstr );

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->so = WWRP<socket_base>(this);
			_cookie->cookie = cookie;
			_cookie->success = fn;
			_cookie->error = err;

			watch(is_wcp(), IOE_WRITE, fd(), _cookie, wawo::net::async_connected, wawo::net::async_connect_error);
		}

		inline void end_async_connect() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_WRITE]);
			_end_async_write();
		}

		inline void _begin_async_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_read, fn_io_event_error const& err = wawo::net::async_error) {
			WAWO_ASSERT(is_nonblocking());

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->cookie = cookie;
			_cookie->so = WWRP<socket_base>(this);

			if (m_rflag&SHUTDOWN_RD) {
				TRACE_IOE("[socket][%s][begin_async_read]cancel for rd shutdowned already", info().to_lencstr().cstr );
				wawo::task::fn_lambda lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};

				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				socket_observer_plan(_t);
				return;
			}

			if (m_rflag&WATCH_READ) {
				TRACE_IOE("[socket][%s][begin_async_read]ignore for watch already", info().to_lencstr().cstr );
				return;
			}

			WAWO_ASSERT(!(m_rflag&WATCH_READ));

			m_rflag |= (WATCH_READ | async_flag);
			TRACE_IOE("[socket][%s][begin_async_read]watch IOE_READ", info().to_lencstr().cstr );

			u8_t flag = IOE_READ;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_READ;
			}
			watch(is_wcp(), flag, fd(), _cookie, fn, err);
		}

		inline void begin_async_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_read, fn_io_event_error const& err = wawo::net::async_error) {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			_begin_async_read(async_flag, cookie, fn, err);
		}

		inline void end_async_read() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
			_end_async_read();
		}

		inline void _begin_async_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_write, fn_io_event_error const& err = wawo::net::async_error) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WWRP<async_cookie> _cookie = wawo::make_ref<async_cookie>();
			_cookie->cookie = cookie;
			_cookie->so = WWRP<socket_base>(this);

			if (m_wflag&SHUTDOWN_WR) {
				TRACE_IOE("[socket][%s][begin_async_write]cancel for wr shutdowned already", info().to_lencstr().cstr );
				wawo::task::fn_lambda lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};
				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				socket_observer_plan(_t);
				return;
			}

			if (m_wflag&WATCH_WRITE) {
				TRACE_IOE("[socket][%s][begin_async_write]ignore for write watch already", info().to_lencstr().cstr );
				return;
			}

			m_wflag |= (WATCH_WRITE | async_flag);
			TRACE_IOE("[socket][%s][begin_async_write]watch IOE_WRITE", info().to_lencstr().cstr );

			u8_t flag = IOE_WRITE;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_WRITE;
			}
			watch(is_wcp(), flag, fd(), _cookie, fn, err);
		}

		inline void begin_async_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn = wawo::net::async_write, fn_io_event_error const& err = wawo::net::async_error) {
			lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
			_begin_async_write(async_flag, cookie, fn, err);
		}

		inline void end_async_write() {
			lock_guard<spin_mutex> lg_r(m_mutexes[L_WRITE]);
			_end_async_write();
		}
	};
}}
#endif