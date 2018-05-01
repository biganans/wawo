#ifndef _WAWO_NET_HANDLER_MUX_HPP
#define _WAWO_NET_HADNLER_MUX_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/bytes_ringbuffer.hpp>
#include <wawo/net/channel.hpp>

#include <wawo/thread/thread.hpp>
#include <wawo/thread/mutex.hpp>

#include <map>

//#define ENABLE_DEBUG_STREAM 1
#ifdef ENABLE_DEBUG_STREAM
#define DEBUG_STREAM WAWO_INFO
#else
#define DEBUG_STREAM(...)
#endif

namespace wawo { namespace net { namespace handler {
	using namespace wawo::thread;

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

	typedef u32_t mux_stream_id_t;
	typedef u16_t mux_stream_frame_flag_t;

	static std::atomic<u32_t> _s_id_{1};
	inline static u32_t mux_make_stream_id() {return wawo::atomic_increment(&_s_id_);}

	enum mux_stream_flag {
		STREAM_READ_SHUTDOWN_CALLED = 1,
		STREAM_READ_SHUTDOWN = 1 << 1,
		STREAM_WRITE_SHUTDOWN_CALLED = 1 << 2,
		STREAM_WRITE_SHUTDOWN = 1 << 3,

		STREAM_READ_CHOKED = 1<<4,

		STREAM_PLAN_FIN = 1 << 4,
		STREAM_PLAN_SYN = 1 << 5,
		STREAM_BUFFER_DETERMINED = 1 << 6,
	};

	enum mux_stream_state {
		SS_CLOSED,
		SS_ESTABLISHED,	//read/write ok
		SS_RECYCLE	//could be removed from stream list
	};

	enum mux_stream_write_flag {
		STREAM_WRITE_BLOCKED = 0x01,
		STREAM_WRITE_UNBLOCKED = 0x02
	};

	static const u32_t MUX_STREAM_WND_SIZE = 256 * 1024;

	class stream :
		public wawo::net::channel
	{
		inline void _snd_fin() {
			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			WAWO_ASSERT(m_id != 0);
			WAWO_ASSERT(!(m_flag&STREAM_WRITE_SHUTDOWN_CALLED));

			WAWO_ASSERT(!(m_flag&STREAM_PLAN_FIN));
			m_flag |= STREAM_PLAN_FIN;
		}

		inline void _snd_syn() {
			WAWO_ASSERT(!(m_flag&STREAM_PLAN_SYN));
			m_flag |= STREAM_PLAN_SYN;
		}
		void _flush(int& ec_o, int& wflag);

	public:
		inline void force_close() {
			lock_guard<spin_mutex> lg(m_mutex);
			{
				lock_guard<spin_mutex> lg_read(m_rb_mutex);
				m_flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);
			}

			{
				lock_guard<spin_mutex> lg_write(m_wb_mutex);
				m_flag |= (STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN);
			}

			m_state = SS_CLOSED;
			DEBUG_STREAM("[mux_cargo][s%u]stream force close", id);
		}

		spin_mutex m_mutex;
		mux_stream_state m_state;
		u8_t m_flag;

		u32_t m_id;
		WWRP<wawo::net::channel_handler_context> m_ch_ctx;

		spin_mutex m_rb_mutex;
		WWRP<wawo::bytes_ringbuffer> m_rb;
		u32_t m_rb_incre;

		spin_mutex m_wb_mutex;
		WWRP<wawo::bytes_ringbuffer> m_wb;
		u32_t m_wb_wnd;
		u8_t m_write_flag;

		stream(WWRP<wawo::net::channel_handler_context> const& ctx):
			m_ch_ctx(ctx),
			m_state(SS_CLOSED),
			m_flag(0),
			m_id(0)
		{}

		~stream()
		{
			//DEBUG_STREAM("stream destruct");
		}

		void init() {
			channel::init();

			m_rb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);
			m_wb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);

			m_rb_incre = 0;
			m_wb_wnd = MUX_STREAM_WND_SIZE;

			m_write_flag = 0;

			m_flag |= STREAM_READ_CHOKED;
		}

		int open(u32_t const& _id, WWRP<ref_base> const& _ctx) {
			WAWO_ASSERT(m_id == 0);
			lock_guard<spin_mutex> lg(m_mutex);
			WAWO_ASSERT(m_state == SS_CLOSED);
			m_id = _id;
			m_state = SS_ESTABLISHED;

			channel::set_ctx(_ctx);
			_snd_syn();
			return wawo::OK;
		}

		int ch_id() const { return m_id; }

		int ch_close( int const& ec) {
			lock_guard<spin_mutex> lg(m_mutex);
			if ((m_flag & (STREAM_READ_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN_CALLED)) ==
				(STREAM_READ_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN_CALLED)
				) {
				return E_CHANNEL_INVALID_STATE;
			}

			{
				lock_guard<spin_mutex> lg_read(m_rb_mutex);
				if (!(m_flag&STREAM_READ_SHUTDOWN_CALLED)) {
					DEBUG_STREAM("[mux_cargo][s%u]close, stream close read", id);
					m_flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);
					if (m_rb->count()) {
						m_rb_incre += m_rb->count();
					}
				}
			}

			{
				lock_guard<spin_mutex> lg_write(m_wb_mutex);
				if (!(m_flag&STREAM_WRITE_SHUTDOWN_CALLED)) {
					DEBUG_STREAM("[mux_cargo][s%u]close, stream close write", id);
					_snd_fin();
					m_flag |= STREAM_WRITE_SHUTDOWN_CALLED;
				}
			}

			return wawo::OK;
		}

		int ch_close_read( int const& ec ) {
			lock_guard<spin_mutex> lg(m_mutex);
			if (m_flag&STREAM_READ_SHUTDOWN_CALLED) {
				return E_CHANNEL_RD_SHUTDOWN_ALREADY;
			}

			DEBUG_STREAM("[mux_cargo][s%u]stream close read", id);
			lock_guard<spin_mutex> lg_read(m_rb_mutex);
			m_flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);

			if (m_rb->count()) {
				m_rb_incre += m_rb->count();
			}
			return wawo::OK;
		}

		int ch_close_write(int const& ec) {
			lock_guard<spin_mutex> lg(m_mutex);
			if (m_flag&STREAM_WRITE_SHUTDOWN_CALLED) {
				return E_CHANNEL_WR_SHUTDOWN_ALREADY;
			}

			lock_guard<spin_mutex> lg_write(m_wb_mutex);
			DEBUG_STREAM("[mux_cargo][s%u]stream close write", id);
			_snd_fin();
			m_flag |= STREAM_WRITE_SHUTDOWN_CALLED;
			return wawo::OK;
		}

		int ch_write(WWRP<packet> const& opack) {
			if (m_flag&(STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN)) {
				return E_CHANNEL_WR_SHUTDOWN_ALREADY;
			}

			lock_guard<spin_mutex> lg_s(m_wb_mutex);
			if (m_wb->left_capacity() < opack->len()) {
				DEBUG_STREAM("[mux_cargo][s%u]stream left_capacity()=%u, try to write: %u", id, wb->left_capacity(), opack->len());
				m_write_flag |= STREAM_WRITE_BLOCKED;
				return wawo::E_CHANNEL_WRITE_BLOCK;
			}

			u32_t nwrites = m_wb->write(opack->begin(), opack->len());
			WAWO_ASSERT(nwrites == opack->len());

			return wawo::OK;
		}

		u32_t read(WWRP<packet>& pack, u32_t const& max = 0) {

			lock_guard<spin_mutex> lg_s(m_rb_mutex);
			if (m_rb->count() == 0) return 0;

			u32_t try_max = ((max != 0) && (m_rb->count() > max)) ? max : m_rb->count();

			WWRP<packet> _pack = wawo::make_ref<wawo::packet>(try_max);
			u32_t rcount = m_rb->read(_pack->begin(), try_max);
			_pack->forward_write_index(rcount);

#ifdef ENABLE_STREAM_WND
			rb_incre += rcount;
#endif
			DEBUG_STREAM("[mux_cargo][s%u]rb_incre: %u, incred: %u", id, rb_incre, rcount);
			pack = _pack;
			return rcount;
		}

		bool is_read_write_closed() {
			lock_guard<spin_mutex> lg(m_mutex);

			return ((m_flag&(STREAM_READ_SHUTDOWN | STREAM_PLAN_FIN)) == (STREAM_READ_SHUTDOWN | STREAM_PLAN_FIN)) ||
				((m_flag&(STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN))
				;
		}

		int update(int& ec_o, int& wflag) {
			switch (m_state) {
			case SS_ESTABLISHED:
			{
				_flush(ec_o, wflag);

				if (ec_o == wawo::OK) {
					break;
				}

				if (ec_o == wawo::E_CHANNEL_WRITE_BLOCK) {
					wflag |= STREAM_WRITE_BLOCKED;
					wawo::this_thread::yield();
					break;
				}

				force_close();
				WAWO_ERR("[mux_cargo]peer socket send error: %d, close peer, force close stream", ec_o);
			}
			break;
			case SS_CLOSED:
			{
				if ((m_flag&(STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) {
					m_state = SS_RECYCLE;
				}
			}
			break;
			case SS_RECYCLE:
			{
			}
			break;
			}

			return m_state;
		}
	};


	class mux:
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract
	{
		typedef std::map<mux_stream_id_t, WWRP<stream> > stream_map_t;
		typedef std::pair<mux_stream_id_t, WWRP<stream> > stream_pair_t;

		wawo::thread::spin_mutex m_mutex;
		stream_map_t m_stream_map;

		WWRP<wawo::net::channel_acceptor_handler_abstract> m_ch_acceptor;

	public:
		mux( WWRP<wawo::net::channel_acceptor_handler_abstract> const& ch_acceptor );
		virtual ~mux();

		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);
		void closed(WWRP<wawo::net::channel_handler_context> const& ctx);
	};
}}}

#endif