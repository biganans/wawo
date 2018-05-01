#include <wawo/net/handler/mux.hpp>

namespace wawo { namespace net { namespace handler {

	void stream_reply_rst(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<stream> const& s) {
		WAWO_ASSERT(!"TODO");
	}

	void stream_snd_rst(WWRP<wawo::net::channel_handler_context> const& ctx, mux_stream_id_t id) {
		WAWO_ASSERT(!"TODO");
	}

	int stream_uwnd(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<stream> const& s,  u32_t const& size) {
		WAWO_ASSERT(size > 0);

		WWRP<packet> o = wawo::make_ref<packet>();
		o->write<u32_t>(size);
		o->write_left<mux_stream_frame_flag_t>(mux_stream_frame_flag::T_UWND);
		o->write_left<mux_stream_id_t>(s->ch_id());

		return ctx->write(o);
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
		mux_stream_frame_flag_t t = income->read<mux_stream_frame_flag_t>();

		if (t >= mux_stream_frame_flag::T_MUX_STREAM_MESSAGE_TYPE_MAX) {
			DEBUG_STREAM("[mux_cargo][s%u][rst]invalid stream message type, ignore", stream_id);
			return;
		}

		WWRP<stream> s;
		if (t == mux_stream_frame_flag::T_SYN ) {
			WAWO_ASSERT(income->len() == 0);
			{
				lock_guard<spin_mutex> lg(m_mutex);
				typename stream_map_t::iterator it = m_stream_map.find(id);
				if (it != m_stream_map.end()) {
					WAWO_WARN("[mux][s%u][syn]stream exists, reply rst", id);
					stream_reply_rst(it->second);
					return;
				}

				s = wawo::make_ref<stream>();
				s->m_state = SS_ESTABLISHED;
				s->ch_id = id;
				s->init();
				m_stream_map.insert({id, s});
				DEBUG_STREAM("[mux_cargo][s%u]stream insert (by syn)", stream_id);
			}

			WAWO_ASSERT(m_ch_acceptor != NULL);
			m_ch_acceptor->accepted(ctx, s);
			s->ch_connected();
			return;
		}

		{
			lock_guard<spin_mutex> lg(m_mutex);
			typename stream_map_t::iterator it = m_stream_map.find(id);
			if (it == m_stream_map.end()) {
				if (t == mux_stream_frame_flag::T_RST) {
					DEBUG_STREAM("[mux_cargo][s%u][rst]stream not found, ignore", stream_id);
					return;
				}

				DEBUG_STREAM("[mux_cargo][s%u][%u]stream not found, reply rst, len: %u", id, t, income->len() );
				stream_snd_rst(id);
				return;
			}

			s = it->second;
		}

		/*
		*
		* please note that:
		* as the stream transfer data upon a reliable & sequence promise channel
		* T_SYN,T_DATA, T_FIN would be transfered in order
		* for T_RST, always do close action;
		*/
		WAWO_ASSERT(s != NULL);

		switch (t) {
		case mux_stream_frame_flag::T_DATA:
			{
				DEBUG_STREAM("[mux_cargo][s%u][data]nbytes: %u", stream_id, inpack->len());
				lock_guard<spin_mutex> lg_stream(s->m_rb_mutex);
				if (s->m_flag&STREAM_READ_SHUTDOWN) {
					stream_snd_rst(s->ch_id);
					DEBUG_STREAM("[mux_cargo][s%u][data]nbytes, but stream read closed, reply rst, incoming bytes: %u", s->ch_id, income->len());
					return;
				}
				if (s->m_flag&STREAM_READ_CHOKED) {
					WAWO_ASSERT(income->len() <= s->m_rb->left_capacity());
					s->m_rb->write(income->begin(), income->len());
					DEBUG_STREAM("[mux_cargo][s%u]incoming: %u, wirte to rb", stream_id, inpack->len());
					return;
				}

				if (s->m_rb->count()) {
					WWRP<packet> tmp = wawo::make_ref<packet>(s->m_rb->count() + income->len() );
					s->m_rb->read(tmp->begin(), tmp->left_left_capacity());
					s->m_rb->skip(tmp->len());
					s->ch_read(tmp);
				} else {
					s->ch_read(income);
				}


			}
			break;
		case mux_stream_frame_flag::T_FIN:
			{
				DEBUG_STREAM("[mux_cargo][s%u][fin]recv", s->ch_id );
				s->ch_close_read(0);
				s->ch_read_shutdowned();
			}
			break;
		case mux_stream_frame_flag::T_RST:
			{
				DEBUG_STREAM("[mux_cargo][s%u][rst]recv, force close", s->ch_id );
				s->force_close();
			}
			break;
		case mux_stream_frame_flag::T_UWND:
			{
				WAWO_ASSERT(income->len() == sizeof(wawo::u32_t));
				wawo::u32_t wnd_incre = income->read<wawo::u32_t>();
				lock_guard<spin_mutex> wb_mutex(s->m_wb_mutex);
				s->m_wb_wnd += wnd_incre;
				s->m_wb_wnd = WAWO_MIN(MUX_STREAM_WND_SIZE, s->m_wb_wnd);

				DEBUG_STREAM("[mux_cargo][s%u]new wb_wnd: %u, incre: %u", s->ch_id, s->wb_wnd, wnd_incre);
			}
			break;
		}
	}

	void mux::closed(WWRP<wawo::net::channel_handler_context> const& ctx) {	
		lock_guard<spin_mutex> lg(m_mutex);
		stream_map_t::iterator it = m_stream_map.begin();
		while (it != m_stream_map.end()) {
			it->second->ch_close(0);
			it->second->ch_closed();
			DEBUG_STREAM("[mux_cargo][s%u][mux_close]force close stream, for E_CLOSE", it->second->id);
			++it;
		}

		int k = 0;
		while (1) {
			lock_guard<spin_mutex> lg_stream_map(m_mutex);
			if (m_stream_map.size() == 0) {
				break;
			}
			else {
				_do_tick();
				wawo::this_thread::yield(++k);
			}
		}
	}

	inline void _do_tick() {

		{
			lock_guard<spin_mutex> lg_outp(m_outps_mutex);
			while (m_outps.size()) {
				WWSP<packet>& outp = m_outps.front();
				int sndrt = peer_abstract_t::m_so->send_packet(outp);
				if (sndrt == wawo::E_SOCKET_SEND_BLOCK) {
					return;
				}
				else if (sndrt == wawo::OK) {
					m_outps.pop();
				}
				else {
					m_outps.pop();
					DEBUG_STREAM("[mux_cargo]flush outps failed: %d, cancel", sndrt);
				}
			}
		}

		bool is_socket_disconnected = peer_abstract::m_so == NULL || !peer_abstract::m_so->is_connected();
		typename stream_map::iterator it = m_stream_map.begin();
		while (it != m_stream_map.end()) {
			WWRP<stream>& s = it->second;

			int ec;
			int wflag = 0;

			if (is_socket_disconnected) {
				DEBUG_STREAM("[mux_cargo][s%u]socket disconnected force close socket", s->id);
				s->_force_close();
			}

			int state = s->update(peer_abstract_t::m_so, ec, wflag);
			typename stream_map::iterator _it = it;
			++it;

			if (state == SS_RECYCLE) {

				DEBUG_STREAM("[mux_cargo][s%u]reach recycle state, schedule evt T_CLOSED", s->id);
				WWSP<message_t> _m = wawo::make_shared<message_t>(s, message::T_CLOSED);
				WWRP<peer_event_t> evt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), peer_abstract_t::m_so, _m);
				_m->ctx = s->ctx;

				DEBUG_STREAM("[mux_cargo][s%u]erase stream by tick", _it->first);
				m_stream_map.erase(_it);

				dispatcher_t::oschedule(evt);
			}
			else {

				if (wflag&STREAM_WRITE_BLOCKED) {
					DEBUG_STREAM("[mux_cargo][s%u]stream write blocked, schedule evt T_WRITE_BLOCK");
					WWSP<message_t> _m = wawo::make_shared<message_t>(s, message::T_WRITE_BLOCK);
					WWRP<peer_event_t> evt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), peer_abstract_t::m_so, _m);
					_m->ctx = s->ctx;
					dispatcher_t::oschedule(evt);
				}

				if (wflag&STREAM_WRITE_UNBLOCKED) {
					DEBUG_STREAM("[mux_cargo][s%u]stream write unblocked, schedule evt T_WRITE_UNBLOCK");
					WWSP<message_t> _m = wawo::make_shared<message_t>(s, message::T_WRITE_UNBLOCK);
					WWRP<peer_event_t> evt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), peer_abstract_t::m_so, _m);
					_m->ctx = s->ctx;
					dispatcher_t::oschedule(evt);
				}
			}
		}
	}

}}}