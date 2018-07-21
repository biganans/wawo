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

#define ENABLE_DEBUG_STREAM 1
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

	static std::atomic<int> __stream_id_{1};
	inline static int mux_make_stream_id() {return wawo::atomic_increment(&__stream_id_)%0x7FFFFFFF;}

	enum mux_stream_flag {
		F_STREAM_READ_CHOKED = (1<<WAWO_CHANNEL_CUSTOM_FLAG_BEGIN),
//		F_STREAM_FLUSH_CHOKED = (1<<(WAWO_CHANNEL_CUSTOM_FLAG_BEGIN+1)),
		F_STREAM_WRITE_FIN_AFTER_WRITE_DONE = (1<<(WAWO_CHANNEL_CUSTOM_FLAG_BEGIN+2))
	};
	static_assert(WAWO_CHANNEL_CUSTOM_FLAG_BEGIN + 3 <= WAWO_CHANNEL_CUSTOM_FLAG_END, "CHANNEL FLAG CHECK FAILED, MAX LEFT SHIFT REACHED");

	enum mux_stream_state {
		SS_CLOSED,
		SS_ESTABLISHED,	//read/write ok
	};

	#define MUX_STREAM_WND_SIZE (96*1024)
	struct mux_stream_frame {
		mux_stream_frame_flag_t flag;
		WWRP<wawo::packet> data;
	};

	struct mux_stream_outbound_entry {
		WWRP<wawo::packet> data;
		WWRP<wawo::net::channel_promise> ch_promise;
	};

	class mux_stream:
		public wawo::net::channel
	{
		friend class mux;
		
		u32_t m_id;
		u32_t m_wnd;

		WWRP<wawo::net::channel_handler_context> m_ch_ctx;
		WWRP<wawo::bytes_ringbuffer> m_rb;

		std::queue<mux_stream_outbound_entry> m_entry_q;
		wawo::size_t m_entry_q_bytes;

		WWRP<channel_promise> m_shutdown_write_promise;
		WWRP<channel_promise> m_close_promise;

		u16_t m_flag;
		u8_t m_state;
		bool m_is_active;

		void _init() {
			m_rb = wawo::make_ref<bytes_ringbuffer>(MUX_STREAM_WND_SIZE);
			channel::ch_fire_opened();
		}
		inline int _ch_do_shutdown_read() {
			WAWO_ASSERT((m_flag&F_READ_SHUTDOWN) == 0);
			m_flag |= F_WRITE_SHUTDOWN;
			return wawo::OK;
		}
		inline int _ch_do_shutdown_write() {
			WAWO_ASSERT((m_flag&F_WRITE_SHUTDOWN) == 0);
			while (m_entry_q.size()) {
				mux_stream_outbound_entry& entry = m_entry_q.front();
				m_entry_q_bytes -= entry.data->len();
				event_poller()->schedule([entry]() {
					entry.ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				m_entry_q.pop();
			}
			m_flag |= F_WRITE_SHUTDOWN;
			return wawo::OK;
		}

	public:
		mux_stream(u32_t const& id, WWRP<wawo::net::channel_handler_context> const& ctx):
			channel(ctx->event_poller()),
			m_id(id),
			m_wnd(MUX_STREAM_WND_SIZE),
			m_ch_ctx(ctx),
			m_entry_q_bytes(0),
			m_flag(0),
			m_state(SS_CLOSED),
			m_is_active(false)
		{
			WAWO_ASSERT(ctx != NULL);
			_init();
		}

		~mux_stream()
		{
		}

		int ch_set_read_buffer_size(u32_t size) {
			//return socket_base::set_rcv_buffer_size(size);
			(void)size;
			return wawo::OK;
		}
		int ch_get_read_buffer_size(u32_t& size) {
			//return socket_base::get_rcv_buffer_size(size);
			(void)size;
			return wawo::OK;
		}
		int ch_set_write_buffer_size(u32_t size) {
			m_wnd = size;
			return wawo::OK;
		}
		int ch_get_write_buffer_size(u32_t& size) {
			size = m_wnd;
			return wawo::OK;
		}

		inline mux_stream_frame make_frame_syn() {
			WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_SYN, o };
		}

		inline mux_stream_frame make_frame_uwnd(u32_t size) {
			WAWO_ASSERT(size > 0);
			WWRP<packet> o = wawo::make_ref<packet>();
			o->write<u32_t>(size);

			return { mux_stream_frame_flag::T_UWND, o };
		}

		inline mux_stream_frame make_frame_rst() {
			WWRP<packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_RST, o };
		}

		inline mux_stream_frame make_frame_data(WWRP<wawo::packet> const& data) {
			WAWO_ASSERT(data != NULL);
			return { mux_stream_frame_flag::T_DATA, data };
		}
		inline mux_stream_frame make_frame_fin() {
			WAWO_ASSERT(m_id != 0);
			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
			return { mux_stream_frame_flag::T_FIN, o };
		}

		inline void _write_frame(mux_stream_frame const& frame, WWRP<wawo::net::channel_promise> const& ch_promise) {
			if (m_flag&F_WRITE_SHUTDOWN) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				return;
			}

			if (m_flag&(F_WRITE_SHUTDOWNING|F_CLOSING)) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if (m_flag&F_WRITE_BLOCKED) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
				});
				return;
			}

			if ( m_entry_q_bytes + frame.data->len() + sizeof(mux_stream_frame_flag_t) + sizeof(u32_t) > m_wnd) {
				m_flag |= F_WRITE_BLOCKED;
				event_poller()->schedule([ch_promise,CH=WWRP<wawo::net::channel>(this)]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
					CH->ch_fire_write_block();
				});
				return;
			}

			frame.data->write_left<mux_stream_frame_flag_t>(frame.flag);
			frame.data->write_left<u32_t>(m_id);

			m_entry_q.push({frame.data,ch_promise});
			m_entry_q_bytes += frame.data->len();
			if((m_flag&F_WATCH_WRITE) != 0 ) {
				return;
			}
			ch_flush_impl();
		}

		inline WWRP<wawo::net::channel_future> write_frame(mux_stream_frame const& frame) {
			WWRP<wawo::net::channel_promise> ch_promise = wawo::make_ref<wawo::net::channel_promise>(WWRP<wawo::net::channel>(this));
			return write_frame(frame, ch_promise);
		}

		inline WWRP<wawo::net::channel_future> write_frame(mux_stream_frame const& frame, WWRP<wawo::net::channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), frame, ch_promise]() {
				S->_write_frame(frame, ch_promise);
			});
			return ch_promise;
		}

		void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(outlet != NULL);
			WAWO_ASSERT(ch_promise != NULL);
			WAWO_ASSERT(outlet->len() > 0);
			_write_frame({T_DATA, outlet}, ch_promise);
		}

		void _ch_write_block_check() {
			WAWO_ASSERT(!"TODO");
			//@todo plan a timer, if the send failed, we'll close this channel for a write timeout errno
		}

		void _ch_flush_done( async_io_result const& r ) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(r.op == AIO_WRITE);
			const int _errno = r.v.code;
			if (_errno == wawo::OK) {
				WAWO_ASSERT(m_entry_q.size());
				mux_stream_outbound_entry& entry = m_entry_q.front();
				WAWO_ASSERT(m_entry_q_bytes >= entry.data->len());

				entry.ch_promise->set_success(wawo::OK);
				m_entry_q_bytes -= entry.data->len();
				m_wnd -= entry.data->len();
				WAWO_ASSERT(m_wnd>0);
				m_entry_q.pop();
				m_flag &= ~F_WATCH_WRITE;

				if (m_entry_q.size()) {
					ch_flush_impl();
				} else {
					if (m_flag&F_WRITE_BLOCKED) {
						m_flag &= ~F_WRITE_BLOCKED;
						event_poller()->schedule([CH=WWRP<channel>(this)]() {
							CH->ch_fire_write_unblock();
						});
					}
					
					if (m_flag&F_STREAM_WRITE_FIN_AFTER_WRITE_DONE) {
						m_flag &= ~F_STREAM_WRITE_FIN_AFTER_WRITE_DONE;
						//WAWO_ASSERT(m_shutdown_write_promise != NULL);
						//f is for call compatible only
						mux_stream_frame frame = make_frame_fin();
						WWRP<channel_promise> f = wawo::make_ref<channel_promise>(WWRP<channel>(this));
						frame.data->write_left<mux_stream_frame_flag_t>(frame.flag);
						frame.data->write_left<u32_t>(m_id);
						m_entry_q.push({ frame.data,f });
						m_entry_q_bytes += frame.data->len();
						ch_flush_impl();
					}
					if (m_flag&F_CLOSING) {
						_ch_do_close_read_write(NULL);
					} else if (m_flag&F_WRITE_SHUTDOWNING) {
						int rt = _ch_do_shutdown_write();
						WAWO_ASSERT(m_shutdown_write_promise != NULL);
						event_poller()->schedule([CHF= m_shutdown_write_promise,rt]() {
							CHF->set_success(rt);
						});
						m_shutdown_write_promise = NULL;
					} else {}
				}
			} else if(_errno == wawo::E_CHANNEL_WRITE_BLOCK) {
				m_flag &= ~F_WATCH_WRITE;
				_ch_write_block_check();
			} else {
				m_flag &= ~F_WATCH_WRITE;
				ch_errno(_errno);
				m_flag |= F_WRITE_ERROR;
				ch_close();
			}
		}

		inline int _do_ch_flush_impl() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT((m_flag&F_WATCH_WRITE) == 0);
			WAWO_ASSERT(m_entry_q.size() != 0);
			while (m_entry_q.size()) {
				WAWO_ASSERT(m_entry_q_bytes > 0);
				mux_stream_outbound_entry& entry = m_entry_q.front();
				if (entry.ch_promise->is_cancelled()) {
					m_entry_q_bytes -= entry.data->len();
					m_entry_q.pop();
					continue;
				}

				WAWO_ASSERT(m_ch_ctx != NULL);
				m_flag |= F_WATCH_WRITE;
				WWRP<channel_future> write_f = m_ch_ctx->write(entry.data);
				write_f->add_listener([S= WWRP<mux_stream>(this)](WWRP<channel_future> const& f) {
					S->_ch_flush_done({ AIO_WRITE, f->get() });
				});
				break;
			}
			return wawo::OK;
		}

		void ch_flush_impl() {
			_do_ch_flush_impl();
		}

		void _dial(fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise ) {
			WAWO_ASSERT(m_state == SS_CLOSED);
			m_is_active= true;

			m_state = SS_ESTABLISHED;
			initializer(WWRP<channel>(this));
			mux_stream_frame syn_frame = make_frame_syn();
			ch_promise->add_listener([CH = WWRP<channel>(this)](WWRP<wawo::net::channel_future> const& f) {
				if (f->get() == wawo::OK) {
					CH->ch_fire_connected();
				}
			});
			write_frame(syn_frame, ch_promise);
		}

		WWRP<wawo::net::channel_future> dial(fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), initializer, ch_promise]() -> void {
				S->_dial(initializer, ch_promise);
			});
			return ch_promise;
		}

		WWRP<wawo::net::channel_future> dial(fn_channel_initializer const& initializer) {
			WWRP<wawo::net::channel_promise> ch_promise = wawo::make_ref<wawo::net::channel_promise>(WWRP<wawo::net::channel>(this));
			return dial(initializer, ch_promise);
		}

		inline void arrive_frame(mux_stream_frame const& frame) {
			switch (frame.flag) {
			case mux_stream_frame_flag::T_DATA:
			{
				WWRP<wawo::packet> income;
				{
					DEBUG_STREAM("[muxs][s%u][data]nbytes: %u", m_id, frame.data->len());
					if (m_flag&F_READ_SHUTDOWN) {
						write_frame(make_frame_rst());
						DEBUG_STREAM("[muxs][s%u][data]nbytes, but stream read closed, reply rst, incoming bytes: %u", m_id, income->len());
						return;
					}

					if (m_flag&F_STREAM_READ_CHOKED) {
						WAWO_ASSERT(frame.data->len() <= m_rb->left_capacity());
						m_rb->write(frame.data->begin(), frame.data->len());
						DEBUG_STREAM("[muxs][s%u]incoming: %u, wirte to rb", m_id, frame.data->len());
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

					WWRP<wawo::net::channel_future> f=write_frame(make_frame_uwnd(income->len()));
					f->add_listener([](WWRP<wawo::net::channel_future> const& f) {
						if (wawo::E_CHANNEL_WRITE_BLOCK == f->get()) {
							WAWO_ASSERT(!"TODO, WE NEED A TIMER");
						}
					});
				}
				WAWO_ASSERT(income->len());
				ch_read(income);
			}
			break;
			case mux_stream_frame_flag::T_FIN:
			{
				DEBUG_STREAM("[muxs][s%u][fin]recv", ch_id() );
				ch_shutdown_read();
			}
			break;
			case mux_stream_frame_flag::T_RST:
			{
				DEBUG_STREAM("[muxs][s%u][rst]recv, force close", ch_id() );
				ch_close();
			}
			break;
			case mux_stream_frame_flag::T_UWND:
			{
				WAWO_ASSERT(frame.data->len() == sizeof(wawo::u32_t));
				wawo::u32_t wnd_incre = frame.data->read<wawo::u32_t>();
				m_wnd += wnd_incre;
				m_wnd = WAWO_MIN(MUX_STREAM_WND_SIZE, m_wnd);
				DEBUG_STREAM("[muxs][s%u]new wb_wnd: %u, incre: %u", ch_id(), m_wnd, wnd_incre);

				if (m_flag&F_WRITE_BLOCKED) {
					if (m_entry_q.size()) {
						WAWO_ASSERT(m_entry_q_bytes > 0);
						WAWO_ASSERT(m_flag&F_WATCH_WRITE);
						//wait next outlet promise set
					} else {
						//manual unchoke
						m_flag &= ~F_WRITE_BLOCKED;
						event_poller()->schedule([CH = WWRP<channel>(this)]() {
							CH->ch_fire_write_unblock();
						});
					}
				}
			}
			break;
			}
		}

		channel_id_t ch_id() const { return m_id; }

		void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());

			if (m_flag&F_READ_SHUTDOWN) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(E_CHANNEL_READ_SHUTDOWN_ALREADY);
				});
				return;
			}

			DEBUG_STREAM("[mux][s%u]stream close read, left buffer count: %u", ch_id(), m_rb->count() );
			_ch_do_shutdown_read();//always return wawo::OK
			event_poller()->schedule([ch_promise,S=WWRP<mux_stream>(this)]() {
				ch_promise->set_success(wawo::OK);
				S->ch_fire_read_shutdowned();
				S->__rdwr_shutdown_check();
			});
		}

		void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());

			if (m_flag&F_WRITE_SHUTDOWN) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				return ;
			}

			if (m_flag&(F_WRITE_SHUTDOWNING)) {
				WAWO_ASSERT(m_entry_q.size() != 0);
				WAWO_ASSERT(m_flag&F_WATCH_WRITE);
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if (m_flag&F_WATCH_WRITE) {
				WAWO_ASSERT((m_flag&F_WRITE_ERROR) == 0);
				m_flag |= (F_WRITE_SHUTDOWNING|F_STREAM_WRITE_FIN_AFTER_WRITE_DONE);
				WAWO_ASSERT(m_shutdown_write_promise == NULL);
				m_shutdown_write_promise = ch_promise;
				return;
			}

			if ((m_flag&F_WRITE_ERROR) == 0) {
				WAWO_ASSERT(m_entry_q.size() == 0);
				WAWO_ASSERT(m_entry_q_bytes == 0);
				m_flag |= F_WRITE_SHUTDOWNING;
				mux_stream_frame f = make_frame_fin();
				write_frame(make_frame_fin());
				return;
			}
			_ch_do_shutdown_write();
		}

		void _ch_do_close_read_write(WWRP<channel_promise> const& close_f) {
			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			m_state = SS_CLOSED;
			if (!(m_flag&F_READ_SHUTDOWN)) {
				_ch_do_shutdown_read();
			}
			if (!(m_flag&F_WRITE_SHUTDOWN)) {
				_ch_do_shutdown_write();//ALWAYS RETURN OK
				if (m_shutdown_write_promise != NULL) {
					event_poller()->schedule([CHP=m_shutdown_write_promise]() {
						CHP->set_success(wawo::OK);
					});
					m_shutdown_write_promise = NULL;
				}
			}
			if (m_close_promise != NULL) {
				event_poller()->schedule([CHP = m_close_promise]() {
					CHP->set_success(wawo::OK);
				});
				m_close_promise = NULL;
			}
			event_poller()->schedule([close_f, CH=WWRP<channel>(this)]() {
				if (close_f != NULL) {
					close_f->set_success(wawo::OK);
				}
				CH->ch_fire_closed(wawo::OK);
			});
		}

		void ch_close_impl(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(event_poller()->in_event_loop());

			if (m_state == SS_CLOSED) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY);
				});
				return;
			}
			WAWO_ASSERT(m_state == SS_ESTABLISHED);

			if (m_flag&F_WATCH_WRITE) {
				m_flag |= (F_CLOSING|F_STREAM_WRITE_FIN_AFTER_WRITE_DONE);
				m_close_promise = ch_promise;
				return;
			}
			if ((m_flag&F_WRITE_ERROR) == 0) {
				WAWO_ASSERT(m_entry_q.size() == 0);
				WAWO_ASSERT(m_entry_q_bytes == 0);

				m_flag |= F_CLOSING;
				mux_stream_frame f = make_frame_fin();
				write_frame(make_frame_fin());
				return;
			}
			_ch_do_close_read_write(ch_promise);
		}

		void __rdwr_shutdown_check() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if ((m_flag&(F_READWRITE_SHUTDOWN)) == F_READWRITE_SHUTDOWN) {
				ch_close();
			}
		}

		void begin_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_read = NULL) {
			WAWO_ASSERT( !(m_flag&F_STREAM_READ_CHOKED) == 0);
			m_flag &= ~F_STREAM_READ_CHOKED;

			(void)async_flag;
			(void)cookie;
			(void)fn_read;
		}

		void end_read() {
			WAWO_ASSERT(m_flag|F_STREAM_READ_CHOKED);
			m_flag |= F_STREAM_READ_CHOKED;
		}

		inline bool is_active() const {
			return (m_is_active == true) ;
		}
	};

	enum mux_ch_event {
		E_MUX_CH_CONNECTED,
		E_MUX_CH_CLOSED,
		E_MUX_CH_STREAM_ACCEPTED
	};

	class mux;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux_stream> const& s)> fn_mux_stream_accepted_t;

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
		void read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx);

		void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		void closed(WWRP<wawo::net::channel_handler_context> const& ctx);

		void _stream_close_check_() {
			stream_map_t::iterator it = m_stream_map.begin();
			while (it != m_stream_map.end()) {
				typename stream_map_t::iterator const _it = it;
				++it;
				if (_it->second->m_state == SS_CLOSED) {
					DEBUG_STREAM("[mux_cargo][s%u]stream remove from map", _it->second->ch_id());
					m_stream_map.erase(_it);
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