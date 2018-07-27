#include <wawo/net/handler/mux.hpp>

namespace wawo { namespace net { namespace handler {

	void stream_snd_rst(WWRP<wawo::net::channel_handler_context> const& ctx, mux_stream_id_t id) {
		mux_stream_frame f = make_frame_rst();
		f.data->write_left<mux_stream_frame_flag_t>(f.flag);
		f.data->write_left<mux_stream_id_t>(id);

		WWRP<wawo::net::channel_future> write_f = ctx->write(f.data);
		write_f->add_listener([]( WWRP<wawo::net::channel_future> const& f ) {
			int rt = f->get();
			if (rt == wawo::E_CHANNEL_WRITE_BLOCK) {
				WAWO_ASSERT(!"WE NEED A TIMER");
			}
		});
	}

	mux::mux():m_last_check_time(0)
	{
	}

	mux::~mux() {
	}

	void mux::read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income) {
	
		WAWO_ASSERT(income != NULL);
		WAWO_ASSERT(income->len() >= mux_stream_frame_header_len);
		mux_stream_id_t id = income->read<mux_stream_id_t>();
		mux_stream_frame_flag_t flag = income->read<mux_stream_frame_flag_t>();
		int32_t wnd = income->read<int32_t>();
		u32_t len = income->read<u32_t>();

		if (flag >= mux_stream_frame_flag::FRAME_MUX_STREAM_MESSAGE_TYPE_MAX) {
			DEBUG_STREAM("[muxs][s%u][rst]invalid stream message type, ignore", id);
			return;
		}
		WAWO_ASSERT(id > 0);
		WAWO_ASSERT(len >= 0);
		WAWO_ASSERT(len == income->len());

		WWRP<mux_stream> s;
		if (flag == mux_stream_frame_flag::FRAME_SYN ) {
			WAWO_ASSERT(income->len() == 0);
			int ec;
			s = open_stream(id, ec);
			if (ec != wawo::OK) {
				WAWO_WARN("[mux][s%u][syn]accept stream failed: %d", id, ec );
				stream_snd_rst(ctx, id);
				return;
			}
			WAWO_ASSERT(wnd > 0);
			s->m_snd_wnd = wnd;
			s->m_rcv_wnd = wnd;
			s->m_rcv_data_incre = wnd;
			s->m_state = SS_ESTABLISHED;

			invoke<fn_mux_stream_accepted_t>(E_MUX_CH_STREAM_ACCEPTED, s );
			s->ch_fire_connected();
			s->begin_read();
			return;
		}

		{
			lock_guard<spin_mutex> lg(m_mutex);
			typename stream_map_t::iterator it = m_stream_map.find(id);
			if (it == m_stream_map.end()) {
				if (flag == mux_stream_frame_flag::FRAME_RST) {
					DEBUG_STREAM("[mux][s%u][rst]stream not found, ignore", id);
					return;
				}

				DEBUG_STREAM("[mux][s%u][%u]stream not found, reply rst, len: %u", id, flag, income->len() );
				stream_snd_rst(ctx,id);
				return;
			}

			s = it->second;
		}

		WAWO_ASSERT(s != NULL);
		s->arrive_frame(flag, wnd, income );
	}

	//@TODO, flush all stream up and down, then do close
	void mux::read_shutdowned(WWRP<wawo::net::channel_handler_context> const& ctx) {
		ctx->shutdown_write();
		ctx->fire_read_shutdowned();
	}

	void mux::connected(WWRP<wawo::net::channel_handler_context> const& ctx) {
		lock_guard<spin_mutex> lg(m_mutex);
		WAWO_ASSERT(m_ch_ctx == NULL);
		m_ch_ctx = ctx;

		invoke<fn_mux_evt_t>(E_MUX_CH_CONNECTED,WWRP<mux>(this) );
		ctx->fire_connected();
	}

	void mux::closed(WWRP<wawo::net::channel_handler_context> const& ctx) {
		lock_guard<spin_mutex> lg(m_mutex);
		stream_map_t::iterator it = m_stream_map.begin();
		while (it != m_stream_map.end()) {
			//@TODO, we have to check read_shutdown,then do flush
			it->second->ch_errno(-1);
			it->second->ch_close();
			DEBUG_STREAM("[mux][s%u][mux_close]mux closed, close all mux_stream", it->second->ch_id() );
			++it;
		}

		invoke<fn_mux_evt_t>(E_MUX_CH_CLOSED, WWRP<mux>(this));
		ctx->fire_closed();
		m_ch_ctx = NULL;
	}

}}}