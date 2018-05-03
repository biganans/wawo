#include <wawo/net/handler/mux.hpp>

namespace wawo { namespace net { namespace handler {

	void stream_snd_rst(WWRP<wawo::net::channel_handler_context> const& ctx, mux_stream_id_t id) {
		WAWO_ASSERT(!"TODO");
	}

	mux::mux(WWRP<wawo::net::channel_acceptor_handler_abstract> const& ch_acceptor):
		m_ch_acceptor(ch_acceptor)
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

		WWRP<stream> s;
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

				s = wawo::make_ref<stream>( id, ctx );
				m_stream_map.insert({id, s});

				DEBUG_STREAM("[mux_cargo][s%u]stream insert (by syn)", stream_id);
			}

			WAWO_ASSERT(m_ch_acceptor != NULL);
			m_ch_handler->accepted(ctx, s);
			s->ch_connected();
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

	void mux::closed(WWRP<wawo::net::channel_handler_context> const& ctx) {
		lock_guard<spin_mutex> lg(m_mutex);
		stream_map_t::iterator it = m_stream_map.begin();
		while (it != m_stream_map.end()) {
			it->second->ch_close();
			DEBUG_STREAM("[mux_cargo][s%u][mux_close]force close stream, for E_CLOSE", it->second->id);
			++it;
		}
	}
}}}