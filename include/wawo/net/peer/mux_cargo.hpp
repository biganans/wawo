#ifndef _WAWO_NET_PEER_STREAM_CARGO_HPP
#define _WAWO_NET_PEER_STREAM_CARGO_HPP

#include <atomic>
#include <vector>
#include <list>
#include <map>

#include <wawo/core.hpp>

#include <wawo/net/peer/_mux.hpp>
#include <wawo/net/peer/message/mux_cargo.hpp>

namespace wawo { namespace net { namespace peer {

	template <class _TLP>
	class mux_cargo:
		public wawo::net::peer_abstract<500000ULL>,
		public dispatcher_abstract< peer_event< mux_cargo<_TLP> > >
	{
		enum _mux_state {
			SOCKET_ATTACHED,
			SOCKET_DEATTACHED,
			SOCKET_CLOSED
		};

	public:
		typedef _TLP TLPT;
		typedef mux_cargo<_TLP> self_t;
		typedef mux_cargo<_TLP> mux_cargo_t;
		typedef wawo::net::peer_abstract<500000ULL> peer_abstract_t;

		typedef listener_abstract<socket_event> socket_event_listener_t;
		typedef dispatcher_abstract< peer_event< self_t > > dispatcher_t;

		typedef peer_event<self_t> peer_event_t;
		typedef message::mux_cargo message_t;

		typedef std::map<stream_id_t, WWRP<stream> > stream_map ;
		typedef std::pair<stream_id_t, WWRP<stream> > stream_pair;

	private:
		stream_map m_stream_map;
		_mux_state m_mux_state;

		spin_mutex m_outps_mutex;
		std::queue<WWSP<packet>> m_outps;
	public:

		mux_cargo():
			peer_abstract_t(),
			m_mux_state(SOCKET_DEATTACHED)
		{}

		~mux_cargo() {}

		virtual void attach_socket(WWRP<socket> const& so) {
			lock_guard<shared_mutex> lg(m_mutex);
			_register_evts(so);
			m_so = so;
			m_mux_state = SOCKET_ATTACHED;
		}

		virtual void detach_socket() {
			lock_guard<shared_mutex> lg(m_mutex);
			if (m_so != NULL) {
				_unregister_evts(m_so);
				m_so = NULL;
			}
			m_mux_state = SOCKET_DEATTACHED;
		}

		/**
		* mux promise to transfer all stream bytes which was delivered before close_write call be called
		*
		*/

		int close(int const& code = 0) {

			{
				lock_guard<shared_mutex> _lg(m_mutex);
				m_mux_state = SOCKET_CLOSED;

				typename stream_map::iterator it = m_stream_map.begin();
				while (it != m_stream_map.end()) {
					it->second->close();
					++it;
				}
			}

			int k = 0;
			while (1) {
				lock_guard<shared_mutex> _lg(m_mutex);
				if (m_stream_map.size() == 0) {
					break;
				} else {
					_do_tick();
					wawo::this_thread::yield(++k);
				}
			}

			if (m_so != NULL) {
				return m_so->close(code);
			}

			return wawo::OK;
		}


		WWRP<stream> stream_open( stream_id_t const& id, WWRP<ref_base> const& ctx, int& ec_o ) {

			ec_o = wawo::OK;

			lock_guard<shared_mutex> _lg(m_mutex);
			if (m_mux_state != SOCKET_ATTACHED) {
				ec_o = wawo::E_INVALID_STATE;
				return NULL;
			}

			typename stream_map::iterator it = m_stream_map.find( id );
			if (it != m_stream_map.end()) {
				ec_o = wawo::E_MUX_STREAM_CARGO_STREAM_EXISTS;
				return NULL;
			}

			WWRP<stream> _stream = wawo::make_ref<stream>();
			int openrt = _stream->open(id,ctx);
			DEBUG_STREAM("[mux_cargo][s%u]stream opening, openrt: %d", id,openrt);
			if (openrt != wawo::OK) {
				ec_o = openrt;
				return NULL;
			}
			_stream->init();

			stream_pair pair(id, _stream);
			m_stream_map.insert(pair);

			DEBUG_STREAM("[mux_cargo][s%u]stream insert", id);
			return _stream ;
		}

		int stream_close(stream_id_t const& stream_id) {
			shared_lock_guard<shared_mutex> lg(m_mutex);
			typename stream_map::iterator it = m_stream_map.find(stream_id);
			if (it == m_stream_map.end()) {
				WAWO_WARN("[mux_cargo][s%u]stream not found", stream_id);
				return E_MUX_STREAM_CARGO_STREAM_NOT_FOUND;
			}
			return it->second->close();
		}

		void _stream_snd_rst(stream_id_t const& id ) {

			WAWO_ASSERT(m_mux_state == SOCKET_ATTACHED);
			WAWO_ASSERT(m_so != NULL);

			WWSP<packet> outp = wawo::make_shared<packet>();
			outp->write_left<message::mux_stream_message_type_t>(message::T_RST&0xFFFF);
			outp->write_left<wawo::net::peer::stream_id_t>(id);
			int sndrt = m_so->send_packet(outp);

			if (sndrt == wawo::E_SOCKET_SEND_BLOCK) {
				lock_guard<spin_mutex> lg_outp(m_outps_mutex);
				m_outps.push(outp);
			}
			DEBUG_STREAM("[mux_cargo][s%u]stream snd rst, sndrt: %d", id, sndrt );
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

			bool is_socket_disconnected = peer_abstract::m_so==NULL || !peer_abstract::m_so->is_connected();
			typename stream_map::iterator it = m_stream_map.begin();
			while (it != m_stream_map.end()) {
				WWRP<stream>& s = it->second;

				int ec;
				int wflag = 0;

				if (is_socket_disconnected) {
					DEBUG_STREAM("[mux_cargo][s%u]socket disconnected force close socket", s->id );
					s->_force_close();
				}

				int state = s->update(peer_abstract_t::m_so, ec, wflag );
				typename stream_map::iterator _it = it;
				++it;

				if (state == SS_RECYCLE) {
					
					DEBUG_STREAM("[mux_cargo][s%u]reach recycle state, schedule evt T_CLOSED", s->id );
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

		virtual void tick(u64_t const& now) {
			lock_guard<shared_mutex> lgmap(m_mutex);
			_do_tick();
			(void)now;
		}

		int do_send_message(WWSP<message_t> const& message) {
			(void)message;
			WAWO_ASSERT( !"please use stream api" );
			return wawo::OK;
		}

		virtual void on_event(WWRP<socket_event> const& evt) {

			switch (evt->id) {
			case E_PACKET_ARRIVE:
				{
					evt->so->begin_async_read();
					
					{
						lock_guard<shared_mutex> lg_self(m_mutex);
						if (m_mux_state != SOCKET_ATTACHED) {
							return;
						}
					}

					WWSP<packet> const& inpack = evt->data;

					WAWO_ASSERT(inpack != NULL);
					WAWO_ASSERT(inpack->len() >= sizeof(stream_id_t) + sizeof(message::mux_stream_message_type_t));

					stream_id_t stream_id = inpack->read<stream_id_t>();
					WAWO_ASSERT(stream_id != 0);

					message::mux_stream_message_type_t t = inpack->read<message::mux_stream_message_type_t>();

					if ( t >= message::T_MUX_STREAM_MESSAGE_TYPE_MAX || t<0 ) {
						DEBUG_STREAM("[mux_cargo][s%u][rst]invalid stream message type, ignore", stream_id);
						return;
					}

					if (t == message::T_SYN)
					{
						WAWO_ASSERT(inpack->len() == 0);
						WWRP<stream> stream_new;
						{
							lock_guard<shared_mutex> lg_self(m_mutex);
							typename stream_map::iterator it = m_stream_map.find(stream_id);
							if (it != m_stream_map.end()) {
								WAWO_WARN("[mux_cargo][s%u][syn]stream exists, reply rst", stream_id);
								_stream_snd_rst(stream_id);
								return;
							}

							WWRP<stream> _stream = wawo::make_ref<stream>();
							_stream->state = SS_ESTABLISHED;
							_stream->id = stream_id;
							_stream->init();

							stream_pair pair(stream_id, _stream);
							m_stream_map.insert(pair);
							stream_new = _stream;
							DEBUG_STREAM("[mux_cargo][s%u]stream insert (by syn)", stream_id);
						}

						WWSP<mux_cargo_t::message_t> _m = wawo::make_shared<mux_cargo_t::message_t>(stream_new, message::T_ACCEPTED);
						WWRP<mux_cargo_t::peer_event_t> pevt = wawo::make_ref<mux_cargo_t::peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), evt->so, _m);
						dispatcher_t::trigger(pevt);
						return;
					}

					WWRP<stream> s;
					{
						lock_guard<shared_mutex> lg_self(m_mutex);
						typename stream_map::iterator it = m_stream_map.find(stream_id);
						if (it == m_stream_map.end()) {
							if (t == message::T_RST) {
								DEBUG_STREAM("[mux_cargo][s%u][rst]stream not found, ignore", stream_id);
								return;
							}

							u32_t len = 0;
							if (inpack != NULL) {
								len = inpack->len();
							}

							DEBUG_STREAM("[mux_cargo][s%u][%u]stream not found, reply rst, len: %u", stream_id, t, len);
							_stream_snd_rst(stream_id);
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

					WAWO_ASSERT( s != NULL );
					if (t == message::T_DATA) {
						DEBUG_STREAM("[mux_cargo][s%u][data]nbytes: %u", stream_id, inpack->len() );

						{
							lock_guard<spin_mutex> lg_stream(s->rb_mutex);
							if (s->flag&STREAM_READ_SHUTDOWN) {
								_stream_snd_rst(stream_id);
								DEBUG_STREAM("[mux_cargo][s%u][data]nbytes, but stream read closed, reply rst, incoming bytes: %u", stream_id, inpack->len());
								return;
							}

							WAWO_ASSERT(inpack->len() <= s->rb->left_capacity());
							WAWO_ASSERT(s->rb->left_capacity() >= inpack->len());
							s->rb->write( inpack->begin(), inpack->len() );
							DEBUG_STREAM("[mux_cargo][s%u]incoming: %u", stream_id, inpack->len() );
						}

						WWSP<message_t> _m = wawo::make_shared<message_t>( s, message::T_DATA);
						WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), evt->so, _m);
						_m->ctx = s->ctx;

						dispatcher_t::trigger(pevt);

						DEBUG_STREAM("[mux_cargo][s%u][data]nbytes trigger: %u", stream_id, inpack->len());
						return;
					}

					if (t == message::T_FIN) {
						DEBUG_STREAM("[mux_cargo][s%u][fin]recv", stream_id);
						WAWO_ASSERT( s->ctx != NULL );
						WWSP<message_t> _m = wawo::make_shared<message_t>(s, message::T_FIN);
						WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<mux_cargo_t>(this), evt->so, _m);
						_m->ctx = s->ctx;
						WAWO_ASSERT(_m->ctx != NULL);

						dispatcher_t::trigger(pevt);
						return;
					}

					if (t == message::T_RST) {
						DEBUG_STREAM("[mux_cargo][s%u][rst]recv, force close", stream_id);
						s->_force_close();
						return;
					}

#ifdef ENABLE_STREAM_WND
					if (t == message::T_UWND) {
						WAWO_ASSERT(inpack->len() == sizeof(wawo::u32_t));
						wawo::u32_t wnd_incre = inpack->read<wawo::u32_t>();
						lock_guard<spin_mutex> wb_mutex(s->wb_mutex);
						s->wb_wnd += wnd_incre;
						s->wb_wnd = WAWO_MIN(MUX_STREAM_WND_SIZE, s->wb_wnd);

						DEBUG_STREAM("[mux_cargo][s%u]new wb_wnd: %u, incre: %u", stream_id, s->wb_wnd, wnd_incre );
					}
#endif
				}
				break;
			case E_CLOSE:
				{
					WAWO_ASSERT(evt->so == m_so);

					std::vector<WWRP<peer_event_t>> evts;
					{
						lock_guard<shared_mutex> lg_stream_map(m_mutex);
						m_mux_state = SOCKET_CLOSED;

						stream_map::iterator it = m_stream_map.begin();
						while (it != m_stream_map.end()) {
							it->second->close();
							DEBUG_STREAM("[mux_cargo][s%u][mux_close]force close stream, for E_CLOSE", it->second->id);
							++it;
						}
					}
					
					int k = 0;
					while(1){
						lock_guard<shared_mutex> lg_stream_map(m_mutex);
						if (m_stream_map.size() == 0) {
							break;
						} else {
							_do_tick();
							wawo::this_thread::yield(++k);
						}
					}

					WWRP<peer_event_t> pevt2 = wawo::make_ref<peer_event_t>(E_CLOSE, WWRP<self_t>(this), evt->so, evt->info);
					dispatcher_t::trigger(pevt2);
				}
				break;
			case E_RD_SHUTDOWN:
			case E_WR_SHUTDOWN:
			case E_WR_BLOCK:
			case E_WR_UNBLOCK:
			case E_ERROR:
				{
					WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(evt->id, WWRP<self_t>(this), evt->so, evt->info);
					dispatcher_t::trigger(pevt);
				}
				break;
			default:
				{
					char tmp[256] = { 0 };
					snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown socket evt: %d", evt->id );
					WAWO_THROW(tmp);
				}
				break;
			}
		}

	};

}}}
#endif
