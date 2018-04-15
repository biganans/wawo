#ifndef _WAWO_NET_HANDLER_MUX_HPP
#define _WAWO_NET_HADNLER_MUX_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>

//#define ENABLE_DEBUG_STREAM 1
#ifdef ENABLE_DEBUG_STREAM
#define DEBUG_STREAM WAWO_INFO
#else
#define DEBUG_STREAM(...)
#endif

namespace wawo { namespace net { namespace handler {

	enum mux_event {
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

	static std::atomic<u32_t> _s_id_{ 1 };
	inline static u32_t make_stream_id() {return wawo::atomic_increment(&_s_id_);}

	enum stream_flag {
		STREAM_READ_SHUTDOWN_CALLED = 1,
		STREAM_READ_SHUTDOWN = 1 << 1,
		STREAM_WRITE_SHUTDOWN_CALLED = 1 << 2,
		STREAM_WRITE_SHUTDOWN = 1 << 3,

		STREAM_PLAN_FIN = 1 << 4,
		STREAM_PLAN_SYN = 1 << 5,
		STREAM_BUFFER_DETERMINED = 1 << 6,
	};

	enum stream_state {
		SS_CLOSED,
		SS_ESTABLISHED,//read/write ok
		SS_RECYCLE		//could be removed from stream list
	};

	enum stream_write_flag {
		STREAM_WRITE_BLOCKED = 0x01,
		STREAM_WRITE_UNBLOCKED = 0x02
	};

	static const u32_t MUX_STREAM_WND_SIZE = 256 * 1024;

	class stream :
		public wawo::ref_base
	{
		WWRP<wawo::net::channel_handler_context> m_ch_ctx;
	public:
		stream(WWRP<wawo::net::channel_handler_context> const& ctx) : m_ch_ctx(ctx)
		{}
		virtual ~stream() {}

		inline void _snd_fin() {
			WAWO_ASSERT(state == SS_ESTABLISHED);
			WAWO_ASSERT(id != 0);
			WAWO_ASSERT(!(flag&STREAM_WRITE_SHUTDOWN_CALLED));

			WAWO_ASSERT(!(flag&STREAM_PLAN_FIN));
			flag |= STREAM_PLAN_FIN;
		}

		inline void _snd_syn() {
			WAWO_ASSERT(!(flag&STREAM_PLAN_SYN));
			flag |= STREAM_PLAN_SYN;
		}

		inline void _force_close() {
			lock_guard<spin_mutex> lg(mutex);
			{
				lock_guard<spin_mutex> lg_read(rb_mutex);
				flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);
			}

			{
				lock_guard<spin_mutex> lg_write(wb_mutex);
				flag |= (STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN);
			}

			state = SS_CLOSED;
			DEBUG_STREAM("[mux_cargo][s%u]stream force close", id);
		}

		void _flush(WWRP<socket> const& so, int& ec_o, int& wflag);

	public:
		WWRP<ref_base> ctx;

		spin_mutex mutex;
		stream_state state;
		u8_t flag;
		u32_t id;

		spin_mutex rb_mutex;
		WWRP<wawo::bytes_ringbuffer> rb;
		u32_t rb_incre;

		spin_mutex wb_mutex;
		WWRP<wawo::bytes_ringbuffer> wb;
		u32_t wb_wnd;
		u8_t write_flag;

		stream() :
			state(SS_CLOSED),
			flag(0),
			id(0)
		{
			//DEBUG_STREAM("stream construct");
		}

		~stream()
		{
			//DEBUG_STREAM("stream destruct");
		}

		void init() {
			rb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);
			wb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);

			rb_incre = 0;
			wb_wnd = MUX_STREAM_WND_SIZE;

			write_flag = 0;
		}

		int write(WWSP<packet> const& opack) {
			if (flag&(STREAM_WRITE_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN)) {
				return E_MUX_STREAM_CARGO_STREAM_WRITE_SHUTDOWNED_ALREADY;
			}

			lock_guard<spin_mutex> lg_s(wb_mutex);
			if (wb->left_capacity() < opack->len()) {
				DEBUG_STREAM("[mux_cargo][s%u]stream left_capacity()=%u, try to write: %u", id, wb->left_capacity(), opack->len());
				write_flag |= STREAM_WRITE_BLOCKED;
				return wawo::E_CHANNEL_WRITE_BLOCK;
			}

			u32_t nwrites = wb->write(opack->begin(), opack->len());
			WAWO_ASSERT(nwrites == opack->len());

			return wawo::OK;
		}

		u32_t read(WWSP<packet>& pack, u32_t const& max = 0) {

			lock_guard<spin_mutex> lg_s(rb_mutex);
			if (rb->count() == 0) return 0;

			u32_t try_max = ((max != 0) && (rb->count() > max)) ? max : rb->count();

			WWSP<packet> _pack = wawo::make_shared<wawo::packet>(try_max);
			u32_t rcount = rb->read(_pack->begin(), try_max);
			_pack->forward_write_index(rcount);

#ifdef ENABLE_STREAM_WND
			rb_incre += rcount;
#endif
			DEBUG_STREAM("[mux_cargo][s%u]rb_incre: %u, incred: %u", id, rb_incre, rcount);
			pack = _pack;
			return rcount;
		}

		int open(u32_t const& _id, WWRP<ref_base> const& _ctx) {
			WAWO_ASSERT(id == 0);
			lock_guard<spin_mutex> lg(mutex);
			WAWO_ASSERT(state == SS_CLOSED);
			id = _id;
			state = SS_ESTABLISHED;
			ctx = _ctx;

			_snd_syn();
			return wawo::OK;
		}

		int close_read() {
			lock_guard<spin_mutex> lg(mutex);
			if (flag&STREAM_READ_SHUTDOWN_CALLED) {
				return E_MUX_STREAM_CARGO_STREAM_READ_SHUTDOWNED_ALREADY;
			}

			DEBUG_STREAM("[mux_cargo][s%u]stream close read", id);
			lock_guard<spin_mutex> lg_read(rb_mutex);
			flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);

			if (rb->count()) {
				rb_incre += rb->count();
			}
			return wawo::OK;
		}

		int close_write() {
			lock_guard<spin_mutex> lg(mutex);
			if (flag&STREAM_WRITE_SHUTDOWN_CALLED) {
				return E_MUX_STREAM_CARGO_STREAM_WRITE_SHUTDOWNED_ALREADY;
			}

			lock_guard<spin_mutex> lg_write(wb_mutex);
			DEBUG_STREAM("[mux_cargo][s%u]stream close write", id);
			_snd_fin();
			flag |= STREAM_WRITE_SHUTDOWN_CALLED;
			return wawo::OK;
		}

		bool is_read_write_closed() {
			lock_guard<spin_mutex> lg(mutex);

			return ((flag&(STREAM_READ_SHUTDOWN | STREAM_PLAN_FIN)) == (STREAM_READ_SHUTDOWN | STREAM_PLAN_FIN)) ||
				((flag&(STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN))
				;
		}

		int close() {
			lock_guard<spin_mutex> lg(mutex);
			if ((flag & (STREAM_READ_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN_CALLED)) ==
				(STREAM_READ_SHUTDOWN_CALLED | STREAM_WRITE_SHUTDOWN_CALLED)
				) {
				return E_MUX_STREAM_CARGO_STREAM_CLOSED_ALREADY;
			}

			{
				lock_guard<spin_mutex> lg_read(rb_mutex);
				if (!(flag&STREAM_READ_SHUTDOWN_CALLED)) {
					DEBUG_STREAM("[mux_cargo][s%u]close, stream close read", id);
					flag |= (STREAM_READ_SHUTDOWN_CALLED | STREAM_READ_SHUTDOWN);
					if (rb->count()) {
						rb_incre += rb->count();
					}
				}
			}

			{
				lock_guard<spin_mutex> lg_write(wb_mutex);
				if (!(flag&STREAM_WRITE_SHUTDOWN_CALLED)) {
					DEBUG_STREAM("[mux_cargo][s%u]close, stream close write", id);
					_snd_fin();
					flag |= STREAM_WRITE_SHUTDOWN_CALLED;
				}
			}

			return wawo::OK;
		}

		int update(WWRP<socket> const& so, int& ec_o, int& wflag) {
			switch (state) {
			case SS_ESTABLISHED:
			{
				_flush(so, ec_o, wflag);

				if (ec_o == wawo::OK) {
					break;
				}

				if (ec_o == wawo::E_CHANNEL_WRITE_BLOCK) {
					wflag |= STREAM_WRITE_BLOCKED;
					wawo::this_thread::yield();
					break;
				}

				so->close(ec_o);
				_force_close();
				WAWO_ERR("[mux_cargo]peer socket send error: %d, close peer, force close stream", ec_o);
			}
			break;
			case SS_CLOSED:
			{
				if ((flag&(STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) == (STREAM_READ_SHUTDOWN | STREAM_WRITE_SHUTDOWN)) {
					state = SS_RECYCLE;
				}
			}
			break;
			case SS_RECYCLE:
			{
			}
			break;
			}

			return state;
		}
	};

	};

	class mux :
		public wawo::net::channel_inbound_handler_abstract
	{
	public:
		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);
	};
}}}

#endif