#include <wawo/net/handler/mux.hpp>

namespace wawo { namespace net { namespace handler {

	void stream_snd_rst(WWRP<wawo::net::channel_handler_context> const& ctx, mux_stream_id_t id) {
		WAWO_ASSERT(!"TODO");
	}

	mux::mux()
	{
	}

	mux::~mux() {
	}

	void mux::read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income) {
	
		WAWO_ASSERT(income != NULL);
		WAWO_ASSERT(income->len() >= sizeof(mux_stream_id_t) + sizeof(mux_stream_frame_flag_t));
		mux_stream_id_t id = income->read<mux_stream_id_t>();
		WAWO_ASSERT(id != 0);
		mux_stream_frame_flag_t flag = income->read<mux_stream_frame_flag_t>();

		if (flag >= mux_stream_frame_flag::T_MUX_STREAM_MESSAGE_TYPE_MAX) {
			DEBUG_STREAM("[mux_cargo][s%u][rst]invalid stream message type, ignore", stream_id);
			return;
		}

		WWRP<mux_stream> s;
		if (flag == mux_stream_frame_flag::T_SYN ) {
			WAWO_ASSERT(income->len() == 0);
			{
				lock_guard<spin_mutex> lg(m_mutex);
				_stream_close_check_();

				typename stream_map_t::iterator it = m_stream_map.find(id);
				if (it != m_stream_map.end() ) {
					WAWO_WARN("[mux][s%u][syn]stream exists, reply rst", id);
					stream_snd_rst( ctx, id );
					return;
				}

				s = wawo::make_ref<mux_stream>( id, ctx );
				s->m_state = SS_ESTABLISHED;
				m_stream_map.insert({id, s});

				DEBUG_STREAM("[mux_cargo][s%u]stream insert (by syn)", stream_id);
			}

			invoke<fn_mux_stream_accepted_t>(E_MUX_CH_STREAM_ACCEPTED,WWRP<mux>(this),s);
			s->ch_fire_connected();
			return;
		}

		{
			lock_guard<spin_mutex> lg(m_mutex);
			typename stream_map_t::iterator it = m_stream_map.find(id);
			if (it == m_stream_map.end()) {
				if (flag == mux_stream_frame_flag::T_RST) {
					DEBUG_STREAM("[mux_cargo][s%u][rst]stream not found, ignore", stream_id);
					return;
				}

				DEBUG_STREAM("[mux_cargo][s%u][%u]stream not found, reply rst, len: %u", id, t, income->len() );
				stream_snd_rst(ctx,id);
				return;
			}

			s = it->second;
		}

		WAWO_ASSERT(s != NULL);
		s->arrive_frame({ flag, income });
	}

	void mux::connected(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_ASSERT(m_ch_ctx == NULL);
		m_ch_ctx = ctx;

		invoke<fn_mux_evt_t>(E_MUX_CH_CONNECTED,WWRP<mux>(this) );
		ctx->fire_connected();
	}

	void mux::closed(WWRP<wawo::net::channel_handler_context> const& ctx) {
		lock_guard<spin_mutex> lg(m_mutex);
		stream_map_t::iterator it = m_stream_map.begin();
		while (it != m_stream_map.end()) {
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>();
			it->second->ch_close(ch_promise);
			DEBUG_STREAM("[mux_cargo][s%u][mux_close]force close stream, for E_CLOSE", it->second->id);
			++it;
		}

		invoke<fn_mux_evt_t>(E_MUX_CH_CLOSED, WWRP<mux>(this));
		ctx->fire_closed();
		m_ch_ctx = NULL;
	}

	void mux::error(WWRP<channel_handler_context> const& ctx) {
		m_ch_ctx = ctx;
		invoke<fn_mux_evt_t>(E_MUX_CH_ERROR, WWRP<mux>(this));
		ctx->fire_error();
	}

}}}