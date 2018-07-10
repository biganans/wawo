#ifndef _WAWO_NET_HANDLER_MUX_HPP
#define _WAWO_NET_HADNLER_MUX_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/bytes_ringbuffer.hpp>
#include <wawo/net/channel.hpp>

#include <wawo/thread.hpp>
#include <wawo/mutex.hpp>

#include <wawo/event_trigger.hpp>
#include <map>

//#define ENABLE_DEBUG_STREAM 1
#ifdef ENABLE_DEBUG_STREAM
	#define DEBUG_STREAM WAWO_INFO
#else
	#define DEBUG_STREAM(...)
#endif

namespace wawo { namespace net { namespace handler {
	

	enum mux_stream_frame_flag {
		T_SYN = 1,
		T_DATA = 1 << 1,
		T_UWND = 1 << 2, //update receiver's wnd to sender
		T_RST = 1 << 3,
		T_FIN = 1 << 4,
		T_ACCEPTED = 1 << 5,
		T_CLOSED = 1 << 6,
		T_WRITE_BLOCK = 1 << 7,
		T_WRITE_UNBLOCK = 1 << 8,
		T_MUX_STREAM_MESSAGE_TYPE_MAX = (T_WRITE_UNBLOCK + 1)
	};

	typedef int mux_stream_id_t;
	typedef u16_t mux_stream_frame_flag_t;

	static std::atomic<int> _s_id_{1};
	inline static int mux_make_stream_id() {return wawo::atomic_increment(&_s_id_)%0x7FFFFFFF;}

	enum mux_stream_flag {
		STREAM_READ_SHUTDOWN_CALLED = 1,
		STREAM_READ_SHUTDOWN = 1 << 1,
		STREAM_READ_CHOKED = 1 << 2,

		STREAM_WRITE_SHUTDOWN_CALLED = 1 << 3,
		STREAM_WRITE_SHUTDOWN = 1 << 4,

		STREAM_PLAN_FIN = 1 << 5,
		STREAM_PLAN_SYN = 1 << 6,
		STREAM_BUFFER_DETERMINED = 1 << 7,

		STREAM_IS_ACTIVE = 1<<8
	};

	enum mux_stream_state {
		SS_CLOSED,
		SS_ESTABLISHED,	//read/write ok
	};

	enum mux_stream_write_flag {
		STREAM_WRITE_BLOCKED = 0x01,
		STREAM_WRITE_UNBLOCKED = 0x02
	};
	static const u32_t MUX_STREAM_WND_SIZE = 256 * 1024;

	struct mux_stream_frame {
		mux_stream_frame_flag_t flag;
		WWRP<wawo::packet> data;
	};

	class mux_stream:
		public wawo::net::channel
	{
		friend class mux;
		
		spin_mutex m_mutex;
		spin_mutex m_rmutex;
		spin_mutex m_wmutex;

		mux_stream_state m_state;
		u16_t m_flag;
		u8_t m_rflag;
		u8_t m_wflag;

		int m_id;
		WWRP<wawo::net::channel_handler_context> m_ch_ctx;

		WWRP<wawo::bytes_ringbuffer> m_rb;
		std::queue<mux_stream_frame> m_wframeq;
		u32_t m_wnd;
		u8_t m_write_flag;

		void _init() {
			m_rb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);
			m_wnd = MUX_STREAM_WND_SIZE;
		}

	public:
		mux_stream(u32_t const& id, WWRP<wawo::net::channel_handler_context> const& ctx):
			channel(ctx->ch->event_poller()),
			m_ch_ctx(ctx),
			m_state(SS_CLOSED),
			m_flag(0),
			m_rflag(0),
			m_wflag(0),
			m_id(id)
		{
			_init();
		}

		~mux_stream()
		{
		}

		inline bool is_read_shutdowned() const { return (m_rflag&STREAM_READ_SHUTDOWN_CALLED) != 0; }
		inline bool is_write_shutdowned() const { return (m_wflag&STREAM_WRITE_SHUTDOWN_CALLED) != 0; }
		inline bool is_readwrite_shutdowned() const { return (((m_rflag | m_wflag)&(STREAM_READ_SHUTDOWN_CALLED|STREAM_WRITE_SHUTDOWN_CALLED)) == (STREAM_READ_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN_CALLED)); }
		inline bool is_closed() const { return (m_state == SS_CLOSED); }

		int dial() {
			lock_guard<spin_mutex> lg(m_mutex);
			WAWO_ASSERT(m_state != SS_ESTABLISHED);
			mux_stream_frame f = make_frame_syn();
			int wrt = write_frame(f);

			WAWO_RETURN_V_IF_NOT_MATCH(wrt, wrt == wawo::OK);
			m_state = SS_ESTABLISHED;
			m_flag |= STREAM_IS_ACTIVE;

			//WWRP<mux_stream> s(this);
			//auto l = [s]() ->void {
			//	s->ch_fire_connected();
			//};
			//WWRP<wawo::task::task> t = wawo::make_ref<wawo::task::task>(l);
			//event_poller()->execute(t);

			channel::ch_fire_connected();

			return wrt;
		}

		//int close() {
		//	return ch_close();
		//}

		void ch_flush_impl() {
			WAWO_ASSERT(!"TODO");
			/*
			lock_guard<spin_mutex> lg(m_wmutex);
			while ( !(m_wflag&STREAM_WRITE_SHUTDOWN_CALLED) &m_wframeq.size() && m_wnd > 0) {
				mux_stream_frame const& f = m_wframeq.front();
				if (f.data->len() > m_wnd) {
					break;
				}

				WWRP<channel_future> ch_future = m_ch_ctx->write(f.data);
				ch_future->add_listener([]() {
				
				});
				if (wrt != wawo::OK) {
					break;
				}
				m_wframeq.pop();
			}
			*/
		}

		inline int write_frame(mux_stream_frame const& frame) {
			lock_guard<spin_mutex> lg(m_wmutex);
			return _write_frame(frame);
		}

		inline int write_frame(mux_stream_frame&& frame) {
			lock_guard<spin_mutex> lg(m_wmutex);
			return _write_frame(frame);
		}

		//write success would make frame data dirty
		inline int _write_frame(mux_stream_frame const& frame) {

			WAWO_ASSERT("@TODO");

			WAWO_ASSERT(m_ch_ctx != NULL);

			if (frame.data->len()>m_wnd) {
				return wawo::E_CHANNEL_WRITE_BLOCK;
			}

			frame.data->write_left(frame.flag);
			frame.data->write_left(m_id);

			int wrt = wawo::OK;
			if (m_wframeq.size()) {
				m_wframeq.push(frame);
				goto end_write_frame;
			}

			m_ch_ctx->write(frame.data);
			if (wrt == wawo::E_CHANNEL_WRITE_BLOCK) {
				m_wframeq.push(frame);
				wrt = wawo::OK;
			}

end_write_frame:
			if (wrt == wawo::OK) {
				if (frame.flag&mux_stream_frame_flag::T_DATA) {
					m_wnd -= (frame.data->len() - sizeof(u32_t) + sizeof(mux_stream_frame_flag_t));
				}
			} else {
				//close for pipe broken
				m_ch_ctx->close();

				//rewind data pointer
				frame.data->skip(sizeof(u32_t) + sizeof(mux_stream_frame_flag_t));
			}
			return wrt;
		}

		inline mux_stream_frame make_frame_fin() {
			WAWO_ASSERT(m_id != 0);
			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			WAWO_ASSERT(!(m_wflag&STREAM_WRITE_SHUTDOWN_CALLED));

			WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_FIN, o };
		}

		inline mux_stream_frame make_frame_syn() {
			WAWO_ASSERT(!(m_wflag&STREAM_WRITE_SHUTDOWN_CALLED));

			WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_SYN, o };
		}

		inline mux_stream_frame make_frame_uwnd(u32_t size) {
			WAWO_ASSERT(size > 0);
			WAWO_ASSERT(!(m_wflag&STREAM_WRITE_SHUTDOWN_CALLED));

			WWRP<packet> o = wawo::make_ref<packet>();
			o->write<u32_t>(size);

			return { mux_stream_frame_flag::T_UWND, o };
		}

		inline mux_stream_frame make_frame_rst() {
			WWRP<packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_RST, o };
		}

		inline mux_stream_frame make_frame_data( WWRP<wawo::packet> const& data ) {
			WAWO_ASSERT(!(m_wflag&STREAM_WRITE_SHUTDOWN_CALLED));
			return { mux_stream_frame_flag::T_DATA, data };
		}

		inline void arrive_frame(mux_stream_frame const& frame) {
			switch (frame.flag) {
			case mux_stream_frame_flag::T_DATA:
			{
				WWRP<wawo::packet> income;
				{
					DEBUG_STREAM("[mux_cargo][s%u][data]nbytes: %u", m_id, frame.data->len());
					lock_guard<spin_mutex> lg_stream(m_rmutex);
					if (m_rflag&STREAM_READ_SHUTDOWN) {
						mux_stream_frame f = make_frame_rst();
						write_frame(f);
						DEBUG_STREAM("[mux_cargo][s%u][data]nbytes, but stream read closed, reply rst, incoming bytes: %u", m_id, income->len());
						return;
					}

					if (m_rflag&STREAM_READ_CHOKED) {
						WAWO_ASSERT(frame.data->len() <= m_rb->left_capacity());
						m_rb->write(frame.data->begin(), frame.data->len());
						DEBUG_STREAM("[mux_cargo][s%u]incoming: %u, wirte to rb", m_id, frame.data->len());
						return;
					}

					if (m_rb->count()) {
						WWRP<packet> tmp = wawo::make_ref<packet>(m_rb->count() + frame.data->len());
						m_rb->read(tmp->begin(), tmp->left_left_capacity());
						m_rb->skip(tmp->len());
						income = tmp;
					} else {
						income = frame.data;
					}

					mux_stream_frame f = make_frame_uwnd(income->len());
					write_frame(f);
				}
				WAWO_ASSERT(income->len());
				ch_read(income);
			}
			break;
			case mux_stream_frame_flag::T_FIN:
			{
				DEBUG_STREAM("[mux_cargo][s%u][fin]recv", s->ch_id);
				WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
				ch_shutdown_read_impl(ch_promise);
				//ch_read_shutdowned();
			}
			break;
			case mux_stream_frame_flag::T_RST:
			{
				DEBUG_STREAM("[mux_cargo][s%u][rst]recv, force close", s->ch_id);
				ch_close();
			}
			break;
			case mux_stream_frame_flag::T_UWND:
			{
				WAWO_ASSERT(frame.data->len() == sizeof(wawo::u32_t));
				wawo::u32_t wnd_incre = frame.data->read<wawo::u32_t>();
				lock_guard<spin_mutex> wb_mutex(m_wmutex);
				m_wnd += wnd_incre;
				m_wnd = WAWO_MIN(MUX_STREAM_WND_SIZE, m_wnd);

				DEBUG_STREAM("[mux_cargo][s%u]new wb_wnd: %u, incre: %u", m_id, s->wb_wnd, wnd_incre);
			}
			break;
			}
		}

		int ch_id() const { return m_id; }

		void ch_close_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());

			if (m_state == SS_CLOSED) {
				ch_promise->set_success(wawo::E_CHANNEL_INVALID_STATE);
				return;
			}

			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			WWRP<channel_promise> ch_promise_r = wawo::make_ref<channel_promise>(WWRP<channel>(this));
			WWRP<channel_promise> ch_promise_w = wawo::make_ref<channel_promise>(WWRP<channel>(this));
			ch_shutdown_read_impl(ch_promise_r);
			ch_shutdown_write_impl(ch_promise_w);

			m_state = SS_CLOSED;
			ch_fire_closed();
			ch_promise->set_success(wawo::OK);
		}

		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise) {
			if (m_rflag&STREAM_READ_SHUTDOWN_CALLED) {
				ch_promise->set_success(E_CHANNEL_RD_SHUTDOWN_ALREADY);
				return;
			}

			DEBUG_STREAM("[mux_cargo][s%u]stream close read", id);
			m_rflag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);

			if (m_rb->count()) {
				int wrt;
				mux_stream_frame f = make_frame_uwnd(m_rb->count());
				do {
					wrt = write_frame(f);
				} while (wrt != wawo::OK);
			}

			ch_fire_read_shutdowned();
			__rdwr_shutdown_check();

			ch_promise->set_success(wawo::OK);
		}

		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (m_wflag&STREAM_WRITE_SHUTDOWN_CALLED) {
				ch_promise->set_success(E_CHANNEL_WR_SHUTDOWN_ALREADY);
				return ;
			}

			//wait until flag changed
			do {
				DEBUG_STREAM("[mux_cargo][s%u]stream close write", id);
				mux_stream_frame f = make_frame_fin();
				int wrt = _write_frame(f);
				if (wrt == wawo::OK) {
					m_wflag |= STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN;
				}
			} while ((!(m_wflag |= STREAM_WRITE_SHUTDOWN_CALLED)));

			ch_fire_read_shutdowned();
			__rdwr_shutdown_check();
			ch_promise->set_success(wawo::OK);
		}

		void __rdwr_shutdown_check() {
			u8_t nflag = 0;
			{
				lock_guard<spin_mutex> lg_write(m_rmutex);
				if (m_rflag&STREAM_READ_SHUTDOWN_CALLED) {
					nflag |= STREAM_READ_SHUTDOWN;
				}
			}
			{
				lock_guard<spin_mutex> lg_write(m_wmutex);
				if (m_wflag& STREAM_WRITE_SHUTDOWN_CALLED) {
					nflag |= STREAM_WRITE_SHUTDOWN;
				}
			}
			if (nflag == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) {
				WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this));
				ch_close(ch_promise);
			}
		}

		void ch_write_impl(WWRP<packet> const& fdata, WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(!"TODO");
			/*
			if (m_wflag&(STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN)) {
				return E_CHANNEL_WR_SHUTDOWN_ALREADY;
			}

			return write_frame( make_frame_data(fdata) );
			*/
		}

		void begin_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_read = NULL) {
			lock_guard<spin_mutex> lg_r(m_rmutex);
			WAWO_ASSERT( !(m_rflag&STREAM_READ_CHOKED) == 0);
			m_rflag &= ~STREAM_READ_CHOKED;

			(void)async_flag;
			(void)cookie;
			(void)fn_read;
		}

		void end_read() {
			lock_guard<spin_mutex> lg_r(m_rmutex);
			WAWO_ASSERT(m_rflag|STREAM_READ_CHOKED);
			m_rflag |= STREAM_READ_CHOKED;
		}

		inline bool is_readwrite_closed() {
			return ((m_rflag|m_wflag)&(STREAM_READ_SHUTDOWN|STREAM_WRITE_SHUTDOWN)) == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN);
		}

		inline bool is_active() const {
			return (m_flag&STREAM_IS_ACTIVE) != 0;
		}

	};

	enum mux_ch_event {
		E_MUX_CH_ERROR,
		E_MUX_CH_CONNECTED,
		E_MUX_CH_CLOSED,
		E_MUX_CH_STREAM_ACCEPTED
	};

	class mux;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void( WWRP<mux> const& mux_, WWRP<mux_stream> const& s)> fn_mux_stream_accepted_t;

	class mux:
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract,
		public event_trigger
	{
		typedef std::map<mux_stream_id_t, WWRP<mux_stream> > stream_map_t;
		typedef std::pair<mux_stream_id_t, WWRP<mux_stream> > stream_pair_t;

		wawo::spin_mutex m_mutex;
		stream_map_t m_stream_map;

		WWRP<wawo::net::channel_handler_context> m_ch_ctx;
	public:
		mux();
		virtual ~mux();

		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);
		void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		void closed(WWRP<wawo::net::channel_handler_context> const& ctx);
		void error(WWRP<channel_handler_context> const& ctx);

		void _stream_close_check_() {
			stream_map_t::iterator it = m_stream_map.begin();
			while (it != m_stream_map.end()) {
				lock_guard<spin_mutex> lgw( it->second->m_mutex );
				typename stream_map_t::iterator const _it = it;
				++it;
				if (_it->second->m_state == SS_CLOSED) {
					m_stream_map.erase(_it);
					DEBUG_STREAM("[mux_cargo][s%u]stream remove from map", _it->second->ch_id() );
				}
			}
		}

		WWRP<mux_stream> make_mux_stream( u32_t id , int& ec) {
			lock_guard<spin_mutex> lg(m_mutex);
			stream_map_t::iterator it = m_stream_map.find(id);
			if (it != m_stream_map.end()) {
				ec = wawo::E_CHANNEL_EXISTS;
				return NULL ;
			}
			ec = wawo::OK;
			WWRP<mux_stream> s = wawo::make_ref<mux_stream>(id,m_ch_ctx);
			m_stream_map.insert({ id, s });
			return s;
		}
	};

}}}

#endif