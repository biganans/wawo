#ifndef _WAWO_NET_ROSNODE_HPP
#define _WAWO_NET_ROSNODE_HPP


#include <wawo/net/service_provider_abstract.hpp>
namespace wawo { namespace net {

	template <class _peer_t>
	class ros_node:
		public service_pool< service_provider_abstract<_peer_t> >,
		public node_abstract<_peer_t>
		{

	public:
		typedef typename node_abstract<_peer_t>::node_t node_t;
		typedef _peer_t peer_t;
		typedef typename peer_t::message_t message_t;
		typedef typename peer_t::peer_event_t peer_event_t;
		typedef service_pool< service_provider_abstract<_peer_t> > service_node_t;
		typedef typename service_node_t::SPT SPT;

		virtual void on_accepted(WWRP<peer_t> const& peer, WWRP<socket> const& so) {
			(void)peer;
			(void)so;

			WAWO_DEBUG("[http_node][%d:%s]local addr: %s, on accepted", so->get_fd(), so->get_addr_info().cstr, so->get_local_addr().address_info().cstr);
		}

		virtual void on_message( WWRP<peer_event_t> const& evt ) {

			WAWO_ASSERT( evt->peer != NULL );
			WAWO_ASSERT( evt->so != NULL );
			WAWO_ASSERT( evt->message != NULL );

			if (evt->message->data->len() < sizeof(service_id_t)) {
				WWSP<packet> outp = wawo::make_shared<packet>();
				outp->write<wawo::CodeT>(wawo::E_PEER_INVALID_REQUEST);
				byte_t unknown[] = "missing service id";
				outp->write((byte_t const* const)unknown, wawo::strlen((char const* const)(unknown)));
				WWSP<message_t> respm = wawo::make_shared<message_t>(outp);
				evt->peer->respond(respm, evt->message );
				return;
			}

			WWSP<packet> const& inpack = evt->message->data;
			service_id_t sid = inpack->read<service_id_t>() ;

			WWRP<SPT> provider = service_node_t::get(sid);

			if( provider == NULL ) {
				WWSP<packet> outp = wawo::make_shared<packet>();
				outp->write<wawo::CodeT>( wawo::E_PEER_INVALID_REQUEST );

				byte_t unknown[] = "service not available";
				outp->write( (byte_t const* const)unknown, wawo::strlen((char const* const)(unknown)) );

				WWSP<message_t> respm = wawo::make_shared< message_t>(outp);
				evt->peer->respond( respm, evt->message ) ;
			} else {
				provider->wait_message( evt->peer, evt->message);
			}
		}
	};
}}
#endif
