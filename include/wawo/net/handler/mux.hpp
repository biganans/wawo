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
		FRAME_SYN = 1,
		FRAME_DATA = 1<<1,
		FRAME_UWND=1<<2, //force update WND
		FRAME_RST = 1<<3,
		FRAME_FIN = 1<<4,
		FRAME_ENCODED = 1<<5, //private flag
		FRAME_MUX_STREAM_MESSAGE_TYPE_MAX = (FRAME_ENCODED + 1)
	};

	typedef int mux_stream_id_t;
	typedef u8_t mux_stream_frame_flag_t;

	//frame format
	//[id:4bytes][flag:1byte][wnd:4bytes][datalen:4bytes][data:variable bytes]
	struct mux_stream_frame_header {
		mux_stream_id_t id;
		mux_stream_frame_flag_t flag;
		u32_t wnd;
		u32_t len;
	};
	static const u8_t mux_stream_frame_header_len = sizeof(mux_stream_id_t) + sizeof(mux_stream_frame_flag_t) + sizeof(int32_t) + sizeof(u32_t);

	static std::atomic<int> __stream_id_{1};
	inline static int mux_make_stream_id() {return wawo::atomic_increment(&__stream_id_)%0x7FFFFFFF;}

	enum mux_stream_flag {
//		F_STREAM_READ_CHOKED = (1<<WAWO_CHANNEL_CUSTOM_FLAG_BEGIN),
	};
	static_assert(WAWO_CHANNEL_CUSTOM_FLAG_BEGIN + 3 <= WAWO_CHANNEL_CUSTOM_FLAG_END, "CHANNEL FLAG CHECK FAILED, MAX LEFT SHIFT REACHED");

	enum mux_stream_state {
		SS_CLOSED,
		SS_ESTABLISHED,	//read/write ok
	};

	#define WAWO_MUX_STREAM_DEFAULT_RVC_SIZE (256*1024)
	struct mux_stream_frame {
		mux_stream_frame_flag_t flag;
		WWRP<wawo::packet> data;
	};

	inline static mux_stream_frame make_frame_syn() {
		WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
		return { mux_stream_frame_flag::FRAME_SYN, o };
	}

	inline static mux_stream_frame make_frame_uwnd() {
		WWRP<packet> o = wawo::make_ref<packet>();
		return { mux_stream_frame_flag::FRAME_UWND, o };
	}

	inline static mux_stream_frame make_frame_rst() {
		WWRP<packet> o = wawo::make_ref<wawo::packet>();
		return { mux_stream_frame_flag::FRAME_RST, o };
	}

	inline static mux_stream_frame make_frame_data(WWRP<wawo::packet> const& data) {
		WAWO_ASSERT(data != NULL);
		return { mux_stream_frame_flag::FRAME_DATA, data };
	}
	inline static mux_stream_frame make_frame_fin() {
		WWRP<wawo::packet> o = wawo::make_ref<wawo::packet>();
		return { mux_stream_frame_flag::FRAME_FIN, o };
	}

	struct mux_stream_outbound_entry {
		mux_stream_frame f;
		WWRP<wawo::net::channel_promise> ch_promise;
	};

	class mux_stream:
		public wawo::net::channel
	{
		friend class mux;
		
		u32_t m_id;
		u32_t m_snd_wnd; //accumulate wnd
		u32_t m_rcv_wnd;
		u32_t m_rcv_data_incre; //rcv incred
		std::queue<WWRP<wawo::packet>> m_incomes_buffer_q;

		WWRP<wawo::net::channel_handler_context> m_ch_ctx;

		std::queue<mux_stream_outbound_entry> m_entry_q;
		wawo::size_t m_entry_q_bytes;

		WWRP<channel_promise> m_shutdown_write_promise;
		WWRP<channel_promise> m_close_promise;
		fn_channel_initializer m_dial_initializer;
		WWRP<channel_promise> m_dial_promise;

		u16_t m_flag;
		u8_t m_state;
		bool m_fin_enqueue_done;
		bool m_is_active;

		void _init() {
			channel::ch_fire_open();
		}
		inline void _ch_do_shutdown_read(WWRP<channel_promise> const& ch_p) {
			WAWO_ASSERT((m_flag&F_READ_SHUTDOWN) == 0);
			end_read();
			m_flag |= F_READ_SHUTDOWN;
			event_poller()->schedule([ch_p, CH = WWRP<channel>(this)]() {
				if (ch_p != NULL) {
					ch_p->set_success(wawo::OK);
				}
				CH->ch_fire_read_shutdowned();
			});
		}
		inline void _ch_do_shutdown_write(WWRP<channel_promise> const& ch_p) {
			DEBUG_STREAM("[muxs][s%u]_ch_do_shutdown_write", m_id );
			WAWO_ASSERT((m_flag&F_WRITE_SHUTDOWN) == 0);
			m_flag |= F_WRITE_SHUTDOWN;
			event_poller()->schedule([ch_p,CH=WWRP<channel>(this)]() {
				if (ch_p != NULL) {
					ch_p->set_success(wawo::OK);
				}
				CH->ch_fire_write_shutdowned();
			});
		}

	public:
		mux_stream(u32_t const& id, WWRP<wawo::net::channel_handler_context> const& ctx) :
			channel(ctx->event_poller()),
			m_id(id),
			m_snd_wnd(0),
			m_rcv_wnd(0),
			m_rcv_data_incre(0),
			m_ch_ctx(ctx),
			m_entry_q_bytes(0),
			m_flag(0),
			m_state(SS_CLOSED),
			m_fin_enqueue_done(false),
			m_is_active(false)
		{
			WAWO_ASSERT(ctx != NULL);
			_init();
			DEBUG_STREAM("mux_stream()");
		}

		~mux_stream() {
			DEBUG_STREAM("~mux_stream()");
		}

		void ch_set_read_buffer_size(u32_t size) {
			ch_set_read_buffer_size(size, make_promise());
		}
		void ch_set_read_buffer_size(u32_t size, WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), size, ch_promise]() {
				int rt = S->_ch_set_read_buffer_size(size);
				ch_promise->set_success(rt);
			});
		}
		void ch_get_read_buffer_size(WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), ch_promise]() {
				int rt = S->_ch_get_read_buffer_size();
				ch_promise->set_success(rt);
			});
		}
		void ch_set_write_buffer_size(u32_t size) {
			ch_set_write_buffer_size(size, make_promise());
		}
		void ch_set_write_buffer_size(u32_t size, WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), size,ch_promise]() {
				int rt = S->_ch_set_write_buffer_size(size);
				ch_promise->set_success(rt);
			});
		}
		void ch_get_write_buffer_size(WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), ch_promise]() {
				int rt = S->_ch_get_write_buffer_size();
				ch_promise->set_success(rt);
			});
		}

		void ch_set_nodelay(WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([S = WWRP<mux_stream>(this), ch_promise]() {
				ch_promise->set_success(wawo::OK);
			});
		}

		int _ch_set_read_buffer_size(u32_t size) {
			if (m_rcv_wnd == size) {
				return wawo::OK;
			}
			if (m_incomes_buffer_q.size() > 0) {
				return wawo::E_INVALID_OPERATION;
			}
			return wawo::OK;
		}
		int _ch_get_read_buffer_size() {
			return m_rcv_wnd;
		}
		int _ch_set_write_buffer_size(u32_t size) {
			DEBUG_STREAM("muxs[s%u]update write buffer from: %u to: %u", m_id, m_snd_wnd, size);
//			m_snd_wnd = size;
			return wawo::OK;
		}
		int _ch_get_write_buffer_size() {
			return m_snd_wnd;
		}

		inline void __push_frame(mux_stream_frame const& frame, WWRP<wawo::net::channel_promise> const& ch_promise) {
			if (WAWO_LIKELY(frame.flag&FRAME_DATA)) {
				m_entry_q_bytes += frame.data->len();
			}
			m_entry_q.push({ frame,ch_promise });
			if ((m_flag&F_WATCH_WRITE) != 0) {
				return;
			}
			ch_flush_impl();
		}

		inline void _write_frame(mux_stream_frame const& frame, WWRP<wawo::net::channel_promise> const& ch_promise) {
			if ((frame.flag&FRAME_DATA)&& (m_flag&F_WRITE_SHUTDOWN)) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				return;
			}

			if ((frame.flag&FRAME_DATA) && m_flag&(F_WRITE_SHUTDOWNING|F_CLOSING)) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if ( (frame.flag&FRAME_DATA) && (m_flag&F_WRITE_BLOCKED) ) {
				DEBUG_STREAM("[muxs][s%u]stream write block, in block,m_snd_wnd: %u, queue bytes: %u, incoming: %u", m_id, m_snd_wnd, m_entry_q_bytes, frame.data->len());
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
				});
				return;
			}

			if ((frame.flag&FRAME_DATA) && m_entry_q_bytes + frame.data->len() > m_snd_wnd) {
				m_flag |= F_WRITE_BLOCKED;
				DEBUG_STREAM("[muxs][s%u]stream write block, set block,m_snd_wnd: %u, queue bytes: %u, incoming: %u", m_id, m_snd_wnd, m_entry_q_bytes, frame.data->len());
				event_poller()->schedule([ch_promise,CH=WWRP<wawo::net::channel>(this)]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_BLOCK);
					CH->ch_fire_write_block();
				});
				return;
			}
			__push_frame(frame, ch_promise);
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
			_write_frame({ FRAME_DATA, outlet}, ch_promise);
		}

		void _ch_write_block_check() {
			WAWO_ASSERT(!"TODO");
			//@todo plan a timer, if the send failed, we'll close this channel for a write timeout errno
		}

		void _ch_flush_done( async_io_result const& r ) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(r.op == AIO_WRITE);
			WAWO_ASSERT(m_flag&F_WATCH_WRITE);
			const int _errno = r.v.code;
			if (_errno == wawo::OK) {
				WAWO_ASSERT(m_entry_q.size());
				const mux_stream_outbound_entry& entry = m_entry_q.front();
				const mux_stream_frame& f = entry.f;
				WAWO_ASSERT(f.flag&FRAME_ENCODED);

				if (f.flag&mux_stream_frame_flag::FRAME_DATA) {
					const u32_t frame_data_len = f.data->len() - mux_stream_frame_header_len;
					WAWO_ASSERT(m_entry_q_bytes >= frame_data_len);
					m_entry_q_bytes -= frame_data_len;

					WAWO_ASSERT(m_snd_wnd >= frame_data_len);
					m_snd_wnd -= frame_data_len;
				}

				m_flag &= ~F_WATCH_WRITE;
				event_poller()->schedule([CHP=entry.ch_promise]() {
					CHP->set_success(wawo::OK);
				});
				m_entry_q.pop();

				if (m_entry_q.size()) {
					ch_flush_impl();
				} else {
					if (m_flag&F_WRITE_BLOCKED) {
						m_flag &= ~F_WRITE_BLOCKED;
						DEBUG_STREAM("[muxs][s%u]stream write unblock, m_snd_wnd: %u",m_id, m_snd_wnd);
						event_poller()->schedule([CH = WWRP<channel>(this)]() {
							CH->ch_fire_write_unblock();
						});
					}

					if (m_flag&F_CLOSING) {
						DEBUG_STREAM("[mux_stream][s%u]_ch_do_close_read_write by F_CLOSING", m_id);
						_ch_do_close_read_write(NULL);
					} else if ( (m_flag&F_WRITE_SHUTDOWNING) && (m_flag&F_WRITE_SHUTDOWN) == 0) {
						DEBUG_STREAM("[mux_stream][s%u]_ch_do_shutdown_write by F_WRITE_SHUTDOWNING", m_id);
						WAWO_ASSERT(m_shutdown_write_promise != NULL);
						_ch_do_shutdown_write(m_shutdown_write_promise);
						m_shutdown_write_promise = NULL;
						__rdwr_shutdown_check();
					} else {
					}
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
				mux_stream_outbound_entry& entry = m_entry_q.front();
				mux_stream_frame& f = entry.f;
				if (entry.ch_promise->is_cancelled()) {
					if (WAWO_LIKELY(f.flag&FRAME_DATA)) {
						m_entry_q_bytes -= f.data->len();
					}
					m_entry_q.pop();
					continue;
				}

				//encoding , and sending
				if ( (f.flag&FRAME_ENCODED) == 0) {
					f.data->write_left<u32_t>(f.data->len());
					f.data->write_left<u32_t>(m_rcv_data_incre);
					f.data->write_left<mux_stream_frame_flag_t>(f.flag);
					f.data->write_left<mux_stream_id_t>(m_id);
					f.flag |= FRAME_ENCODED;
					DEBUG_STREAM("[muxs][s%u]rewind m_rcv_data_incre from: %u to 0", m_id, m_rcv_data_incre);
					m_rcv_data_incre = 0; //rewind wnd incre
				}

				WAWO_ASSERT(m_ch_ctx != NULL);
				m_flag |= F_WATCH_WRITE;
				WWRP<channel_future> write_f = m_ch_ctx->write(f.data);
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

		void _dial_ok() {
			WAWO_ASSERT(m_ch_ctx != NULL);
			WAWO_ASSERT(m_ch_ctx->event_poller()->in_event_loop());
			WAWO_ASSERT(m_state == SS_CLOSED);
			m_is_active = true;
			m_state = SS_ESTABLISHED;
			WAWO_ASSERT(m_dial_initializer != NULL);
			m_dial_initializer(WWRP<channel>(this));
			m_dial_initializer = NULL;
		}

		void _dial_failed() {
			WAWO_ASSERT(m_dial_initializer != NULL);
			m_dial_initializer = NULL;
		}

		void _dial(fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise ) {

			m_snd_wnd = WAWO_MUX_STREAM_DEFAULT_RVC_SIZE;
			m_rcv_wnd = WAWO_MUX_STREAM_DEFAULT_RVC_SIZE;
			m_rcv_data_incre = m_rcv_wnd;
			m_dial_initializer = initializer;
			m_dial_promise = ch_promise;

			WWRP<channel_future> write_f = write_frame(make_frame_syn());
			write_f->add_listener([s=WWRP<mux_stream>(this), ch_promise](WWRP<wawo::net::channel_future> const& f) {
				int rt = f->get();
				s->event_poller()->schedule([s,ch_promise,rt]() {
					if (rt == wawo::OK) {
						s->_dial_ok();
						ch_promise->set_success(wawo::OK);
						s->ch_fire_connected();
						s->begin_read();
					} else {
						s->_dial_failed();
						ch_promise->set_success(rt);
						s->ch_close();
					}
				});
			});
		}

		WWRP<wawo::net::channel_future> dial(fn_channel_initializer const& initializer, WWRP<channel_promise> const& ch_promise) {
			event_poller()->execute([s = WWRP<mux_stream>(this), initializer, ch_promise]() -> void {
				s->_dial(initializer, ch_promise);
			});
			return ch_promise;
		}

		WWRP<wawo::net::channel_future> dial(fn_channel_initializer const& initializer) {
			WWRP<wawo::net::channel_promise> ch_promise = wawo::make_ref<wawo::net::channel_promise>(WWRP<wawo::net::channel>(this));
			return dial(initializer, ch_promise);
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

			DEBUG_STREAM("[mux][s%u]stream close read, left read packet: %u", ch_id(), m_incomes_buffer_q.size() );
			_ch_do_shutdown_read(ch_promise);//always return wawo::OK
			__rdwr_shutdown_check();
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
				WAWO_ASSERT(m_fin_enqueue_done == true);
				WAWO_ASSERT(m_entry_q.size() != 0);
				WAWO_ASSERT(m_flag&F_WATCH_WRITE);
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWNING);
				});
				return;
			}

			if ((m_flag&F_WRITE_ERROR) != 0) {
				WAWO_ASSERT(ch_get_errno() != wawo::OK);
				WAWO_ASSERT(m_entry_q.size() != 0);
				_ch_do_shutdown_write(ch_promise);
				__rdwr_shutdown_check();
				return;
			}

			WAWO_ASSERT(m_shutdown_write_promise == NULL);
			WAWO_ASSERT(ch_promise != NULL);

			WAWO_ASSERT(m_fin_enqueue_done == false);
			m_fin_enqueue_done = true;
			DEBUG_STREAM("[mux_stream][s%u]write fin by shutdown_write", m_id);
			m_flag |= F_WRITE_SHUTDOWNING;
			m_shutdown_write_promise = ch_promise;
			__push_frame( make_frame_fin(),make_promise() );
		}

		void _ch_do_close_read_write(WWRP<channel_promise> const& close_f) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			WAWO_ASSERT(m_state == SS_ESTABLISHED);
			DEBUG_STREAM("[mux_stream][s%u]close mux_stream", m_id);
			m_state = SS_CLOSED;
			if (!(m_flag&F_READ_SHUTDOWN)) {
				_ch_do_shutdown_read(NULL);
			}
			if (!(m_flag&F_WRITE_SHUTDOWN)) {
				_ch_do_shutdown_write(m_shutdown_write_promise);//ALWAYS RETURN OK
				m_shutdown_write_promise = NULL;
			}

			while (m_entry_q.size()) {
				const mux_stream_outbound_entry& f_entry = m_entry_q.front();
				const mux_stream_frame& f = f_entry.f;

				DEBUG_STREAM("[muxs][s%u]write cancel, bytes: %u", m_id, f.data->len());
				if (f.flag&FRAME_DATA) {
					m_entry_q_bytes -= f.data->len();
				}

				event_poller()->schedule([f_entry]() {
					f_entry.ch_promise->set_success(wawo::E_CHANNEL_WRITE_SHUTDOWN_ALREADY);
				});
				m_entry_q.pop();
			}

			if (m_close_promise != NULL) {
				event_poller()->schedule([CHP=m_close_promise]() {
					CHP->set_success(wawo::OK);
				});
				m_close_promise = NULL;
			}
			event_poller()->schedule([close_f, CH = WWRP<channel>(this)]() {
				if (close_f != NULL) {
					close_f->set_success(wawo::OK);
				}
				CH->ch_fire_close(wawo::OK);
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
			if ((m_flag&(F_WRITE_ERROR)) != 0) {
				WAWO_ASSERT(ch_get_errno() != wawo::OK);
				WAWO_ASSERT(m_entry_q.size() != 0);
				_ch_do_close_read_write(ch_promise);
				return;
			}
			if (m_flag&F_CLOSING) {
				event_poller()->schedule([ch_promise]() {
					ch_promise->set_success(wawo::E_CHANNEL_CLOSING);
				});
				return;
			}

			if (m_fin_enqueue_done == false) {
				DEBUG_STREAM("[mux_stream][s%u]write fin by close", m_id);
				WAWO_ASSERT((m_flag&F_CLOSING)==0);

				m_fin_enqueue_done = true;
				m_flag |= F_CLOSING;
				WAWO_ASSERT(m_close_promise == NULL);
				m_close_promise = ch_promise;
				__push_frame(make_frame_fin(), make_promise());
				return;
			}

			if (m_flag&F_WATCH_WRITE) {
				m_flag |= F_CLOSING;
				WAWO_ASSERT(m_fin_enqueue_done == true);
				WAWO_ASSERT(m_entry_q.size() != 0);
				WAWO_ASSERT(m_flag&F_WATCH_WRITE);
				return;
			}

			WAWO_ASSERT(m_fin_enqueue_done == true);
			WAWO_ASSERT(m_entry_q_bytes == 0);
			WAWO_ASSERT(m_entry_q.size() == 0);
			_ch_do_close_read_write(ch_promise);
			return;
		}

		void __rdwr_shutdown_check() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			if (m_state != SS_CLOSED && (m_flag&(F_READWRITE_SHUTDOWN)) == F_READWRITE_SHUTDOWN) {
				event_poller()->schedule([CH=WWRP<channel>(this)]() {
					CH->ch_close();
				});
			}
		}

		void __check_incomes_buffer() {
			WAWO_ASSERT(event_poller()->in_event_loop());
			while ((m_flag&F_WATCH_READ) && m_incomes_buffer_q.size()) {
				const WWRP<wawo::packet>& income = m_incomes_buffer_q.front();
				m_rcv_data_incre += income->len();
				ch_read(income);
				m_incomes_buffer_q.pop();
			}
			
			if ( m_state == SS_ESTABLISHED && m_rcv_data_incre > 0 && m_entry_q.size() == 0) {
				DEBUG_STREAM("[mux][s%u]push uwnd by begin read", m_id );
				__push_frame(make_frame_uwnd(), make_promise());
			}
		}

		void begin_read(u8_t const& async_flag = 0, fn_io_event const& fn_read = NULL) {
			if (!event_poller()->in_event_loop()) {
				event_poller()->schedule([ch = WWRP<channel>(this)](){
					ch->begin_read();
				});
			}
			if (m_flag&F_READ_SHUTDOWN) {
				return;
			}
			WAWO_ASSERT( (m_flag&F_WATCH_READ) == 0 );
			m_flag |= F_WATCH_READ;

			if(m_incomes_buffer_q.size()) {
				event_poller()->schedule([s=WWRP<mux_stream>()]() {
					s->__check_incomes_buffer();
				});
			}
			(void)async_flag;
			(void)fn_read;
		}

		void end_read() {
			if (!event_poller()->in_event_loop()) {
				event_poller()->schedule([ch = WWRP<channel>(this)](){
					ch->end_read();
				});
			}
			m_flag &= ~F_WATCH_READ;
		}

		void begin_write(fn_io_event const& fn_write = NULL) {
			WAWO_ASSERT(!"TODO");
		}
		void end_write() {
			WAWO_ASSERT(!"TODO");
		}

		bool ch_is_active() const {
			return (m_is_active == true) ;
		}

		inline void arrive_frame(mux_stream_frame_flag_t flag,u32_t wnd, WWRP<wawo::packet> const& data ) {
			WAWO_ASSERT(event_poller()->in_event_loop());
			switch (flag) {
			case mux_stream_frame_flag::FRAME_DATA:
			{
				if (m_flag&F_READ_SHUTDOWN) {
					m_rcv_data_incre += data->len();
					DEBUG_STREAM("[muxs][s%u][data]nbytes, but stream read closed,ignore data", m_id, data->len());
					break;
				}
_BEGIN:
				if (!(m_flag&F_WATCH_READ)) {
					m_incomes_buffer_q.push(data);
					DEBUG_STREAM("[muxs][s%u]incoming: %u, buffer to q", m_id, data->len());
					break;
				}
				if (m_incomes_buffer_q.size()) {
					const WWRP<wawo::packet>& income = m_incomes_buffer_q.front();
					m_rcv_data_incre += income->len();
					ch_read(income);
					m_incomes_buffer_q.pop();
					goto _BEGIN;
				}
				ch_read(data);
				m_rcv_data_incre += data->len();
			}
			break;
			case mux_stream_frame_flag::FRAME_FIN:
			{
				DEBUG_STREAM("[muxs][s%u][fin]recv", ch_id());
				ch_shutdown_read();
			}
			break;
			case mux_stream_frame_flag::FRAME_UWND:
			{
				DEBUG_STREAM("[muxs][s%u][uwnd]incre: %u", ch_id(), wnd );
			}
			break;
			case mux_stream_frame_flag::FRAME_RST:
			{
				DEBUG_STREAM("[muxs][s%u][rst]recv, force close", ch_id());
				ch_errno(-2);
				ch_close();
			}
			break;
			}
			//check wnd
			if (wnd>0) {
				m_snd_wnd += wnd;
				DEBUG_STREAM("[muxs][s%u][wndupdate] old: %u, add: %u, new: %u", ch_id(), m_snd_wnd, wnd, m_snd_wnd + wnd);
				if ((m_flag&F_WRITE_BLOCKED)) {
					m_flag &= ~F_WRITE_BLOCKED;
					event_poller()->schedule([CH = WWRP<channel>(this)]() {
						CH->ch_fire_write_unblock();
					});
				}
			}

			if (m_state == SS_ESTABLISHED && (m_rcv_data_incre>0) && (m_entry_q.size()==0) ) {
				//DEBUG_STREAM("[muxs][s%u]push uwnd by arrive, incre: %u", m_id, m_rcv_data_incre );
				__push_frame(make_frame_uwnd(), make_promise());
			}
		}
	};

	enum mux_ch_event {
		E_MUX_CH_CONNECTED,
		E_MUX_CH_CLOSED,
		E_MUX_CH_STREAM_ACCEPTED
	};

	typedef std::function<void(WWRP<mux> const& mux_)> fn_mux_evt_t;
	typedef std::function<void(WWRP<mux_stream> const& s)> fn_mux_stream_accepted_t;

	class mux :
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract,
		public event_trigger
	{
		typedef std::map<mux_stream_id_t, WWRP<mux_stream> > stream_map_t;
		typedef std::pair<mux_stream_id_t, WWRP<mux_stream> > stream_pair_t;

		wawo::spin_mutex m_mutex;
		stream_map_t m_stream_map;
		u64_t m_last_check_time;

		WWRP<wawo::net::channel_handler_context> m_ch_ctx;
	public:
		mux();
		virtual ~mux();

		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);
		void read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx);

		void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		void closed(WWRP<wawo::net::channel_handler_context> const& ctx);

		void write_block(WWRP<channel_handler_context> const& ctx) {
			WAWO_ASSERT(!"TODO");
		}

		void write_unblock(WWRP<channel_handler_context> const& ctx) {
			WAWO_ASSERT(!"TODO");
		}

		WWRP<channel_future> dial_stream(u32_t id, fn_channel_initializer const& initializer) {
			int ec;
			WWRP<mux_stream> S = open_stream(id,ec);
			if (ec == wawo::OK) {
				return S->dial(initializer);
			}
			WWRP<wawo::net::channel_promise> f = wawo::make_ref<wawo::net::channel_promise>(nullptr);
			io_event_loop_group::instance()->schedule([f,ec]() {
				f->set_success(ec);
			});
			return f;
		}

		WWRP<mux_stream> open_stream(u32_t id, int& ec) {
			lock_guard<spin_mutex> lg(m_mutex);
			ec = wawo::OK;
			if (m_ch_ctx == NULL) {
				ec = wawo::E_CHANNEL_CLOSED_ALREADY;
				return NULL;
			}
			stream_map_t::iterator it = m_stream_map.find(id);
			if (it != m_stream_map.end()) {
				ec = wawo::E_CHANNEL_EXISTS;
				return NULL;
			}
		
			WWRP<mux_stream> S = wawo::make_ref<mux_stream>(id, m_ch_ctx);
			m_stream_map.insert({ id, S });
			DEBUG_STREAM("[mux][s%u]insert stream", id);
			S->ch_close_future()->add_listener([M=WWRP<mux>(this), id](WWRP<wawo::net::channel_future> const& f) {
				M->close_stream(id);
				(void)f;
			});
			return S;
		}

		void close_stream(u32_t id) {
			lock_guard<spin_mutex> lg(m_mutex);
			WAWO_ASSERT(id > 0);
			::size_t c = m_stream_map.erase(id);
			DEBUG_STREAM("[mux][s%u]erase stream, erase count: %d", id, c );
			(void)c;
		}
	};
}}}
#endif