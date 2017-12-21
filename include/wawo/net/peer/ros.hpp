#ifndef _WAWO_NET_PEER_WAWO_HPP_
#define _WAWO_NET_PEER_WAWO_HPP_

#include <wawo/thread/mutex.hpp>

#include <wawo/net/tlp/hlen_packet.hpp>
#include <wawo/net/peer/message/ros.hpp>
#include <wawo/net/peer_abstract.hpp>

namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;
	using namespace wawo::net;

	/*
	 * Note: (u'd better read this, if you have any question about wawo::net::peer::wawo )
	 *
	 * wawo::net::peer::wawo is a sub class of peer_abstract
	 * peer_abstract is is what as it is, a object who can send , receive messages by its sockets
	 * it's API is simple as below

	 *
	 * wawo's peer impl do not have a status of login,this is left for yourself
	 * there are several ways to impl a login logic, if you have no clue where to start, pls read the lines below:

	 * First solution
	 * 1, extend wawo::net::peer::wawo, add a state of login|logout, impl your own login message, switch to login state when you get right login response success
	   2, check this state for every message out and in

	 * Seconds solution
	 * 1, add kinds authorize message, once one authorize for some kinds of access is granted, add it to your permisson available table
	 * 2, check permission scope for each message out and in
	 */

	struct ros_callback_abstract :
		virtual public wawo::ref_base
	{
		virtual void on_respond( wawo::net::peer::message::net_id_t const& net_id, WWSP<wawo::packet> const& data ) = 0;
		virtual void on_error(wawo::net::peer::message::net_id_t const& net_id, int const& ec) = 0;
	};

	struct requested_message
	{
		WWSP<message::ros> message;
		WWRP<wawo::net::peer::ros_callback_abstract> cb;
		u64_t ts_request;
		u32_t timeout;
	};

	template <class _TLP = tlp::hlen_packet, u32_t _ticker_freq = 1000*1000*10/*every 10 milisecond*/, int __=0>
	class ros:
		public peer_abstract<_ticker_freq>,
		public dispatcher_abstract< peer_event< ros<_TLP,_ticker_freq,__> > >
	{

	public:
		typedef _TLP TLPT;
		typedef ros<TLPT, _ticker_freq,__> self_t;
		typedef peer_abstract<_ticker_freq> peer_abstract_t;
		typedef typename peer_abstract_t::socket_event_listener_t socket_event_listener_t;

		typedef typename message::ros message_t;
		typedef peer_event<self_t> peer_event_t;
		typedef dispatcher_abstract< peer_event_t > dispatcher_t;

	private:
		typedef std::vector< requested_message > requested_message_pool ;

		spin_mutex m_requested_mutex;
		requested_message_pool m_requested ;

	public:
		explicit ros()
		{
		}

		virtual ~ros() {
			_cancel_all_request();
		}

		void _cancel_all_request( int const& ec = 0 ) {
			lock_guard<spin_mutex> _lg( m_requested_mutex );
			std::for_each( m_requested.begin(), m_requested.end(), [&ec]( requested_message const& req ) {
				wawo::net::peer::ros_callback_abstract* cb = req.cb.get() ;

				if(cb != NULL) {
					cb->on_error(req.message->net_id,ec);
				}
			});
			m_requested.clear();
		}

		void _check_requested_message(u64_t const& now /*in milli*/) {

			lock_guard<spin_mutex> _lg(m_requested_mutex);
			typename requested_message_pool::iterator it = m_requested.begin();

			while (it != m_requested.end()) {
				if (now>=(it->ts_request+it->timeout)) {
					WAWO_NOTICE("[ros]mid: %u, message timeout, delta: %d", it->message->net_id, (now-it->ts_request));
					wawo::net::peer::ros_callback_abstract* cb = it->cb.get();
					if (cb != NULL) {
						cb->on_error( it->message->net_id, wawo::E_PEER_MESSAGE_REQUEST_TIMEOUT);
					}
					it = m_requested.erase(it);
				} else {
					++it;
				}
			}
		}

		virtual void detach_socket() {
			lock_guard<shared_mutex> lg( peer_abstract_t::m_mutex);
			if (peer_abstract_t::m_so != NULL) {
				peer_abstract_t::_unregister_evts(peer_abstract_t::m_so);
				peer_abstract_t::m_so = NULL;
			}
			_cancel_all_request(wawo::E_PEER_NO_SOCKET_ATTACHED);
		}

		virtual void tick( u64_t const& now ) {
			_check_requested_message(now/1000);
		}

		int send( WWSP<message_t> const& message ) {

			WAWO_ASSERT( message != NULL);
			WAWO_ASSERT( message->type == wawo::net::peer::message::ros::T_NONE );

			message->type = wawo::net::peer::message::ros::T_SEND ;
			return self_t::do_send_message( message );
		}

		int request( WWSP<message_t> const& message, WWRP<wawo::net::peer::ros_callback_abstract> const& cb, u32_t const& timeout = 30*1000 /*in ms*/ ) {
			WAWO_ASSERT( message != NULL);
			WAWO_ASSERT( cb != NULL );

			WAWO_ASSERT( message->type == wawo::net::peer::message::ros::T_NONE );
			message->type = wawo::net::peer::message::ros::T_REQUEST ;

			requested_message requested = { message, cb, wawo::time::curr_milliseconds(), timeout };
			//response could arrive before enqueue, so we must push_back before send_packet
			{
				lock_guard<spin_mutex> _lg( m_requested_mutex );
				m_requested.push_back( requested );
			}

			int msndrt = self_t::do_send_message( message );
			WAWO_RETURN_V_IF_MATCH(msndrt, msndrt == wawo::OK);

			lock_guard<spin_mutex> _lg( m_requested_mutex );
			typename requested_message_pool::iterator it = std::find_if( m_requested.begin(), m_requested.end(), [&message]( requested_message const& req ){
				return req.message == message;
			});
			WAWO_CONDITION_CHECK( it != m_requested.end() );
			m_requested.erase( it );

			return msndrt;
		}

		int respond( WWSP<message_t> const& response, WWSP<message_t> const& incoming ) {

			WAWO_ASSERT( response != NULL);
			WAWO_ASSERT( incoming != NULL);

			WAWO_ASSERT( response->type == wawo::net::peer::message::ros::T_NONE );
			WAWO_ASSERT( incoming->type == wawo::net::peer::message::ros::T_REQUEST );

			response->type = wawo::net::peer::message::ros::T_RESPONSE ;
			response->net_id = incoming->net_id ;

			return self_t::do_send_message( response );
		}

		virtual void on_event( WWRP<socket_event> const& evt) {

			WAWO_ASSERT(evt->so == peer_abstract_t::m_so );
			u32_t const& id = evt->id;

			switch( id ) {

			case E_PACKET_ARRIVE:
				{
					evt->so->begin_async_read();

					WWSP<packet> const& inpack = evt->data;
					try {
						int message_parse_ec;
						WWSP<message_t> arrives[5];
						u32_t count = _decode_message_from_packet(inpack, arrives, 5, message_parse_ec);
						if (message_parse_ec != wawo::OK) {
							WAWO_WARN("[ros]message parse error: %d", message_parse_ec);
						}
						WAWO_ASSERT(count <= 5);

						//parse message from packet, and trigger message
						for (u32_t i = 0; i<count; i++) {
							handle_incoming_message(evt->so, arrives[i]);
						}
					} catch( wawo::exception& e ) {
						WAWO_ERR("[wawo_peer]decode message exception: [%d]%s\n%s(%d)%s\n%s, close socket:#%d", e.code, e.message,e.file,e.line,e.func, e.callstack, evt->so->get_fd() );
						evt->so->close(e.code);
					} catch(...) {
						WAWO_ERR("[wawo_peer]decode message exception unknown, close socket:#%d", evt->so->get_fd());
						evt->so->close();
					}
				}
				break;
			case E_RD_SHUTDOWN:
			case E_WR_SHUTDOWN:
			case E_WR_BLOCK:
			case E_WR_UNBLOCK:
			case E_CLOSE:
			case E_ERROR:
				{
					WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(evt->id, WWRP<self_t>(this), evt->so, evt->info);
					dispatcher_t::trigger(pevt);
				}
				break;
			default:
				{
					char tmp[256]={0};
					snprintf( tmp, sizeof(tmp)/sizeof(tmp[0]), "unknown socket evt: %d", id );
					WAWO_THROW( tmp );
				}
				break;
			}
		}

		int do_send_message( WWSP<message_t> const& message ) {
			shared_lock_guard<shared_mutex> lg(peer_abstract_t::m_mutex);
			if( peer_abstract_t::m_so == NULL ) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<packet> packet_mo;
			int encode_ec = message->encode( packet_mo ) ;

			if( encode_ec != wawo::OK ) {
				return encode_ec;
			}

			WAWO_ASSERT(packet_mo != NULL);
			return peer_abstract_t::m_so->send_packet(packet_mo);
		}

		//for blocking mode
		u32_t do_receive_messasges(WWSP<message_t> messages[], u32_t const& size, int& ec) {

			WWRP<wawo::net::socket> recvso;
			u32_t mcount = 0;
			WWSP<message_t> m[5];

			{
				shared_lock_guard<shared_mutex> slg(peer_abstract_t::m_mutex);
				recvso = peer_abstract_t::m_so;
				if (peer_abstract_t::m_so == NULL) {
					ec = wawo::E_PEER_NO_SOCKET_ATTACHED;
					return 0;
				}

				WWSP<packet> arrives[1];
				u32_t pcount;
				do {
					pcount = peer_abstract_t::m_so->receive_packets(arrives, 1, ec);
				} while ((ec == wawo::OK || ec == wawo::E_SOCKET_RECV_BLOCK) && pcount == 0);
				WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);

				WAWO_ASSERT(pcount == 1);
				WAWO_ASSERT(arrives[0]->len() > 0);

				int message_parse_ec;
				mcount = _decode_message_from_packet(arrives[0], m, 5, message_parse_ec);
				if (message_parse_ec != wawo::OK) {
					WAWO_WARN("[ros]message parse error: %d", message_parse_ec);
				}
				WAWO_ASSERT(mcount <= size);
			}

			int midx_o = 0;
			for ( wawo::u32_t i = 0; (i<mcount) && (i < (sizeof(m) / sizeof(m[0])) ); ++i) {
				if (m[i]->type == wawo::net::peer::message::ros::T_RESPONSE ) {
					_handle_resp_message(recvso, m[i]);
				} else {
					messages[midx_o++] = m[i];
				}
			}
			return midx_o;
		}

		inline u32_t _decode_message_from_packet( WWSP<packet> const& inpack, WWSP<message::ros> messages_o[], u32_t const& size, int& ec_o ) {
			ec_o = wawo::OK;
			u32_t count = 0 ;
			do {

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
				u32_t mlength = (inpack->read<wawo::u16_t>()&0xFFFF) ;
#else
				u32_t mlength = inpack->read<wawo::u32_t>() ;
#endif
				WAWO_CONDITION_CHECK(inpack->len() >= mlength ) ;

				if( mlength == inpack->len() ) {
					message::ros::net_id_t net_id = inpack->read<message::ros::net_id_t>();
					message::ros::ros_type type = static_cast<message::ros::ros_type>(inpack->read<message::ros::type_t>());
					//for the case on net_message one packet
					messages_o[count++] = wawo::make_shared<message::ros> ( net_id,type, inpack );
					break;
				} else {
					//one packet , mul-messages
					//copy and assign
					WAWO_ASSERT( "@TODO" );
				}
			} while(inpack->len() && (count<size) );

			return count ;
		}

		inline void _handle_resp_message(WWRP<socket> const& so, WWSP<message_t> const& resp ) {
			WAWO_ASSERT( so != NULL );
			WAWO_ASSERT(resp->type == wawo::net::peer::message::ros::T_RESPONSE );

			requested_message rm;
			rm.cb = NULL;
			{
				lock_guard<spin_mutex> _lg(m_requested_mutex);
				typename requested_message_pool::iterator it = std::find_if(m_requested.begin(), m_requested.end(), [&resp](requested_message const& check_req) {
					return check_req.message->net_id == resp->net_id;
				});

				if (it == m_requested.end() ) {
					WAWO_NOTICE("[ros]mid: %u, we get a resp message that not in request queue, may be timout triggered before", resp->net_id);
					return;
				}

				rm.cb = it->cb;
				rm.message = it->message;
				m_requested.erase(it);
			}

			if (rm.cb != NULL) {
				WAWO_ASSERT( rm.message->net_id == resp->net_id );
				WAWO_ASSERT( resp->type == message::ros::T_RESPONSE );
				WAWO_ASSERT( rm.message->type == message::ros::T_REQUEST );

				(rm.cb)->on_respond( rm.message->net_id, resp->data );
			}
		}

		inline void handle_incoming_message( WWRP<socket> const& so, WWSP<message_t> const& message ) {

			switch( message->type ) {
			case wawo::net::peer::message::ros::T_SEND:
			case wawo::net::peer::message::ros::T_REQUEST:
				{
					WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<self_t>(this), so, message );
					dispatcher_t::trigger(pevt);
				}
				break;
			case wawo::net::peer::message::ros::T_RESPONSE:
				{
					_handle_resp_message(so, message);
				}
				break;
			default:
				{
					WAWO_THROW("invalid message type");
				}
				break;
			}
		}
	};
}}}
#endif
