#ifndef _WAWO_NET_SOCKET_HPP_
#define _WAWO_NET_SOCKET_HPP_

#include <queue>

#include <wawo/smart_ptr.hpp>
#include <wawo/time/time.hpp>
#include <wawo/thread/mutex.hpp>

#include <wawo/packet.hpp>
#include <wawo/net/address.hpp>

#include <wawo/net/socket_base.hpp>
#include <wawo/net/channel.hpp>

#define WAWO_MAX_ASYNC_WRITE_PERIOD	(90000L) //90 seconds

namespace wawo { namespace net {

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

	struct socket_outbound_entry {
		WWRP<packet> data;
		WWRP<channel_promise> ch_promise;
	};

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

//		spin_mutex m_rps_q_mutex;
//		spin_mutex m_rps_q_standby_mutex;

//		std::queue<WWRP<wawo::packet>> *m_rps_q;
//		std::queue<WWRP<wawo::packet>> *m_rps_q_standby;

		fn_io_event m_fn_async_connected;
		fn_io_event_error m_fn_async_connect_error;

		fn_io_event m_fn_async_read;
		fn_io_event_error m_fn_async_read_error;

		fn_io_event m_fn_async_write;
		fn_io_event_error m_fn_async_write_error;

		std::queue<socket_outbound_entry> m_outbound_entry_q;

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
			m_async_wt(0)
		{
			channel::ch_opened();
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
			m_async_wt(0)
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
			m_async_wt(0)
		{
			_init();
		}

		~socket()
		{
			_deinit();
		}
		
		inline bool is_connected() const { return m_state == S_CONNECTED; }
		inline bool is_connecting() const { return (m_state == S_CONNECTING); }

		inline bool is_read_shutdowned() const { return (m_rflag&SHUTDOWN_RD) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&SHUTDOWN_WR) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag | m_wflag)&SHUTDOWN_RDWR) == SHUTDOWN_RDWR); }
		inline bool is_closed() const { return (m_state == S_CLOSED); }

		inline int turnon_nodelay() { return socket_base::turnon_nodelay(); }
		inline int turnoff_nodelay() { return socket_base::turnoff_nodelay(); }

		inline bool is_active() const { return socket_base::is_active(); }

		int open();
		int connect(address const& addr);

		u32_t accept(WWRP<socket> sockets[], u32_t const& size, int& ec_o);

		void ch_bind(wawo::net::address const& address, WWRP<channel_promise> const& ch_promise);
		void ch_listen( WWRP<channel_promise> const& ch_promise, int const& backlog = 128 );

		int async_connect(address const& addr);



		inline void handle_async_connected() {
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			m_state = S_CONNECTED;

			channel::ch_connected();
			begin_read(WATCH_OPTION_INFINITE);
		}

		inline void handle_async_connect_error( int e) {
			WAWO_ASSERT(is_nonblocking());
			WAWO_ASSERT(m_state == S_CONNECTING);
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
			ch_close(ch_promise);
		}

		void handle_async_accept(int& ec_o);
		void handle_async_read(int& ec_o);

	private:
		inline void __rdwr_check() {
			WAWO_ASSERT(event_loop()->in_event_loop());
			u8_t nflag = 0;
			if (m_rflag& SHUTDOWN_RD) {
				nflag |= SHUTDOWN_RD;
			}
			
			if (m_wflag& SHUTDOWN_WR) {
				nflag |= SHUTDOWN_WR;
			}
			if (nflag == SHUTDOWN_RDWR) {
				WWRP<channel_promise> ch_prommise = wawo::make_ref<channel_promise>();
				ch_close(ch_prommise);
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
			if ((m_rflag&WATCH_READ) && is_nonblocking()) {
				m_rflag &= ~(WATCH_READ | WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_read]unwatch IOE_READ", info().to_lencstr()().cstr);
				event_loop()->unwatch(IOE_READ, fd() );
			}
		}

		inline void end_write() {
			if ((m_wflag&WATCH_WRITE) && is_nonblocking()) {
				m_wflag &= ~(WATCH_WRITE | WATCH_OPTION_INFINITE);
				TRACE_IOE("[socket][%s][end_write]unwatch IOE_WRITE", info().to_lencstr().cstr );
				event_loop()->unwatch(IOE_WRITE, fd());
			}
		}

		inline void begin_connect( WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_connected = NULL, fn_io_event_error const& fn_err = NULL ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WAWO_ASSERT(!(m_wflag&SHUTDOWN_WR));
			WAWO_ASSERT(!(m_wflag&WATCH_WRITE));
			m_wflag |= WATCH_WRITE;
			TRACE_IOE("[socket][%s][begin_connect]watch IOE_WRITE", info().to_lencstr().cstr );

			//update default
			if (WAWO_UNLIKELY(fn_connected != NULL)) {
				m_fn_async_connected = fn_connected;
			}
			if (WAWO_UNLIKELY(fn_err != NULL)) {
				m_fn_async_connect_error = fn_err;
			}

			WWRP<ref_base> _cookie = cookie == NULL ? WWRP<socket>(this) : cookie;
			WAWO_ASSERT(m_fn_async_connected != NULL);
			WAWO_ASSERT(m_fn_async_connect_error != NULL);
			event_loop()->watch( IOE_WRITE, fd(), _cookie, m_fn_async_connected, m_fn_async_connect_error );
		}

		inline void end_connect() {
			end_write();
		}

		inline void begin_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
			WAWO_ASSERT(is_nonblocking());

			WWRP<ref_base> _cookie = cookie == NULL ? WWRP<socket>(this) : cookie;
			if (WAWO_UNLIKELY(fn_read != NULL)) {
				m_fn_async_read = fn_read;
			}
			if (WAWO_UNLIKELY(fn_err != NULL)) {
				m_fn_async_read_error = fn_err;
			}

			if (m_rflag&SHUTDOWN_RD) {
				TRACE_IOE("[socket][%s][begin_read]cancel for rd shutdowned already", info().to_lencstr().cstr );
				WAWO_ASSERT(m_fn_async_read_error != NULL);
				fn_io_event_error err = m_fn_async_read_error;
				wawo::task::fn_lambda lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};

				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				event_loop()->schedule(_t);
				return;
			}

			if (m_rflag&WATCH_READ) {
				TRACE_IOE("[socket][%s][begin_read]ignore for watch already", info().to_lencstr().cstr );
				return;
			}

			WAWO_ASSERT(!(m_rflag&WATCH_READ));

			m_rflag |= (WATCH_READ | async_flag);
			TRACE_IOE("[socket][%s][begin_read]watch IOE_READ", info().to_lencstr().cstr );

			u8_t flag = IOE_READ;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_READ;
			}

			WAWO_ASSERT(m_fn_async_read != NULL);
			WAWO_ASSERT(m_fn_async_read_error != NULL);
			event_loop()->watch( flag, fd(), _cookie, m_fn_async_read, m_fn_async_read_error );
		}

		inline void begin_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL ) {
			WAWO_ASSERT(m_state == S_CONNECTED || m_state == S_CONNECTING);
			WAWO_ASSERT(is_nonblocking());

			WWRP<ref_base> _cookie = cookie == NULL ? WWRP<socket>(this) : cookie;

			if (WAWO_UNLIKELY(fn_write != NULL)) {
				m_fn_async_write = fn_write;
			}
			if (WAWO_UNLIKELY(fn_err != NULL)) {
				m_fn_async_write_error = fn_err;
			}

			if (m_wflag&SHUTDOWN_WR) {
				TRACE_IOE("[socket][%s][begin_write]cancel for wr shutdowned already", info().to_lencstr().cstr );
				WAWO_ASSERT(m_fn_async_read_error != NULL);
				fn_io_event_error err = m_fn_async_read_error;
				wawo::task::fn_lambda lambda = [err, _cookie]() -> void {
					err(wawo::E_INVALID_STATE, _cookie);
				};
				WWRP<wawo::task::lambda_task> _t = wawo::make_ref<wawo::task::lambda_task>(lambda);
				event_loop()->schedule(_t);
				return;
			}

			if (m_wflag&WATCH_WRITE) {
				TRACE_IOE("[socket][%s][begin_write]ignore for write watch already", info().to_lencstr().cstr );
				return;
			}

			m_wflag |= (WATCH_WRITE | async_flag);
			TRACE_IOE("[socket][%s][begin_write]watch IOE_WRITE", info().to_lencstr().cstr );

			u8_t flag = IOE_WRITE;
			if (async_flag&WATCH_OPTION_INFINITE) {
				flag |= IOE_INFINITE_WATCH_WRITE;
			}

			WAWO_ASSERT(m_fn_async_write != NULL);
			WAWO_ASSERT(m_fn_async_write_error != NULL);

			event_loop()->watch( flag, fd(), _cookie, m_fn_async_write, m_fn_async_write_error);
		}

		inline int ch_id() const { return fd(); }
		void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_loop()->in_event_loop());
			if ((m_wflag&SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			m_outbound_entry_q.push({
				outlet,
				ch_promise
			});
		}

		void ch_flush_impl()
		{
			WAWO_ASSERT(event_loop()->in_event_loop());
			int _errno = 0;
			while (m_outbound_entry_q.size()) {
				socket_outbound_entry& entry = m_outbound_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					continue;
				}

				u32_t sent = socket_base::send(entry.data->begin(), entry.data->len(), _errno);
				if (sent == entry.data->len()) {
					WAWO_ASSERT(_errno == wawo::OK);
					entry.ch_promise->set_success(wawo::OK);
					m_outbound_entry_q.pop();
					continue;
				}
				entry.data->skip(sent);
				WAWO_ASSERT(_errno != wawo::OK);
				break;
			}
			if (_errno == wawo::OK) {
				WAWO_ASSERT(m_outbound_entry_q.size() == 0);
				if (m_wflag&WATCH_WRITE) {
					end_write();
					channel::ch_write_unblock();
				}
			} else if (_errno == wawo::E_CHANNEL_WRITE_BLOCK ) {
				if ((m_wflag&WATCH_WRITE) == 0) {
					begin_write();
					channel::ch_write_block();
				} else {
					//clear old before setup
					end_write();
					begin_write();
				}
			} else {
				while (m_outbound_entry_q.size()) {
					socket_outbound_entry& entry = m_outbound_entry_q.front();
					entry.ch_promise->set_success(_errno);
					m_outbound_entry_q.pop();
				}
				WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
				ch_close(ch_promise);
			}
		}
		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_loop()->in_event_loop());
			if ((m_rflag&SHUTDOWN_RD) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			m_rflag |= SHUTDOWN_RD;
			int rt = socket_base::shutdown(SHUT_RD);
			end_read();
			channel::ch_read_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}
		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_loop()->in_event_loop());
			if ((m_wflag&SHUTDOWN_WR) != 0) {
				ch_promise->set_success(wawo::E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return;
			}
			ch_flush_impl();
			m_wflag |= SHUTDOWN_WR;
			int rt = socket_base::shutdown(SHUT_WR);
			end_write();
			channel::ch_write_shutdowned();
			__rdwr_check();
			ch_promise->set_success(rt);
		}

		void ch_close_impl(WWRP<channel_promise> const& ch_promise)
		{
			WAWO_ASSERT(event_loop()->in_event_loop());
			if (m_state == S_CLOSED) {
				ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				return;
			}

			int rt = socket_base::close();
			if (is_listener()) {
				end_read();
				m_state = S_CLOSED;
				channel::ch_closed();
				channel::ch_close_promise()->set_success(rt);
				ch_promise->set_success(rt);
				return;
			}

			if (m_state != S_CONNECTED) {
				m_state = S_CLOSED;
				channel::ch_closed();
				channel::ch_close_promise()->set_success(rt);
				ch_promise->set_success(rt);
				return;
			}

			m_state = S_CLOSED;
			if (!(m_rflag&SHUTDOWN_RD)) {
				m_rflag |= SHUTDOWN_RD;
				channel::ch_read_shutdowned();
				end_read();
			}

			if (!(m_wflag&SHUTDOWN_WR)) {
				ch_flush_impl();
				m_wflag |= SHUTDOWN_WR;
				end_write();
			}

			channel::ch_closed();
			channel::ch_close_promise()->set_success(rt);
			ch_promise->set_success(rt);
		}
	};
}}
#endif