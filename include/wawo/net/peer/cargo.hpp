#ifndef _WAWO_NET_PEER_CARGO_HPP
#define _WAWO_NET_PEER_CARGO_HPP

#include <wawo/net/tlp/hlen_packet.hpp>
#include <wawo/net/listener_abstract.hpp>
#include <wawo/net/dispatcher_abstract.hpp>

#include <wawo/net/peer_abstract.hpp>
#include <wawo/net/peer/message/cargo.hpp>

namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;
	using namespace wawo::net;

	template <class _TLP = tlp::hlen_packet, u64_t _ticker_freq = 0, int __tag=0>
	class cargo:
		public peer_abstract<_ticker_freq>,
		public dispatcher_abstract< peer_event< cargo<_TLP, _ticker_freq, __tag> > >
	{

	public:
		typedef _TLP TLPT;
		typedef cargo<TLPT, _ticker_freq,__tag> self_t;
		typedef peer_abstract<_ticker_freq> peer_abstract_t;
		typedef typename peer_abstract_t::socket_event_listener_t socket_event_listener_t;
	public:
		typedef message::cargo message_t;
		typedef peer_event< self_t > peer_event_t;
		typedef dispatcher_abstract< peer_event_t > dispatcher_t;

	public:
		cargo() {}
		virtual ~cargo() {}

		int do_send_message( WWSP<message_t> const& message ) {
			shared_lock_guard<shared_mutex> slg( peer_abstract_t::m_mutex);

			if( peer_abstract_t::m_so== NULL) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<packet> packet_mo;
			int encode_ec = message->encode( packet_mo ) ;

			if( encode_ec != wawo::OK ) {
				return encode_ec;
			}

			WAWO_ASSERT( packet_mo != NULL );
			return peer_abstract_t::m_so->send_packet( packet_mo );
		}

		u32_t do_receive_messasges(WWSP<message_t> message[], u32_t const& size, int& ec ) {
			shared_lock_guard<shared_mutex> slg(peer_abstract_t::m_mutex);
			if (peer_abstract_t::m_so == NULL) {
				ec = wawo::E_PEER_NO_SOCKET_ATTACHED;
				return 0;
			}

			WWSP<packet> arrives[1];
			u32_t pcount;
			do {
				pcount = peer_abstract_t::m_so->receive_packets(arrives, 1, ec);
			} while ((ec==wawo::OK || ec == wawo::E_SOCKET_RECV_BLOCK)&&pcount==0);

			WAWO_RETURN_V_IF_NOT_MATCH(ec, (ec==wawo::OK || ec == wawo::E_TLP_TRY_AGAIN) );

			WAWO_ASSERT( pcount == 1 );
			WAWO_ASSERT(arrives[0]->len()>0 );

			WWSP<message_t> _m = wawo::make_shared<message_t>(arrives[0]);
			message[0] = _m;
			return 1;
		}

		virtual void on_event( WWRP<socket_event> const& evt) {

			switch(evt->id) {

			case E_PACKET_ARRIVE:
				{
					WWSP<packet> const& inpack = evt->data;
					WWRP<socket> const& so = evt->so;

					WWSP<message_t> message = wawo::make_shared<message_t>(inpack) ;
					WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<self_t>(this), so, message) ;
					dispatcher_t::trigger(pevt);
				}
				break;
			case E_RD_SHUTDOWN:
			case E_WR_SHUTDOWN:
			case E_CLOSE:
			case E_WR_BLOCK:
			case E_WR_UNBLOCK:
			case E_ERROR:
				{
					WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(evt->id, WWRP<self_t>(this), evt->so , evt->info) ;
					dispatcher_t::trigger(pevt);
				}
				break;
			default:
				{
					char tmp[256]={0};
					snprintf( tmp, sizeof(tmp)/sizeof(tmp[0]), "unknown socket evt: %d", evt->id );
					WAWO_THROW( tmp );
				}
				break;
			}
		}
	};

}}}
#endif
