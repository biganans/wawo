#ifndef _WAWO_NET_WAWONODE_HPP
#define _WAWO_NET_WAWONODE_HPP

#include <wawo/net/ServiceNode.hpp>

namespace wawo { namespace net {

	template <class _PeerT>
	class WawoNode:
		public ServiceNode<_PeerT>
	{

	public:
		typedef _PeerT PeerT;
		typedef typename PeerT::MessageT MessageT;
		typedef typename PeerT::PeerEventT PeerEventT;
		typedef ServiceNode<_PeerT> ServiceNodeT;
		typedef typename ServiceNode<_PeerT>::SPT SPT;

		virtual void OnPeerMessage( WWRP<PeerEventT> const& evt ) {

			WAWO_ASSERT( evt->GetPeer() != NULL );
			WAWO_ASSERT( evt->GetSocket() != NULL );
			WAWO_ASSERT( evt->GetIncoming() != NULL );

			if (evt->GetIncoming()->GetPacket()->Length() < sizeof(ServiceIdT)) {
				WWSP<Packet> packet(new Packet());
				packet->Write<wawo::CodeT>(wawo::E_PEER_INVALID_REQUEST);
				byte_t unknown[] = "missing service id";
				packet->Write((byte_t const* const)unknown, wawo::strlen((char const* const)(unknown)));
				WWSP<MessageT> respm(new MessageT(packet));
				evt->GetPeer()->Respond(respm, evt->GetIncoming() );
				return;
			}

			WWSP<Packet> const& inpack = evt->GetIncoming()->GetPacket();
			ServiceIdT sid = inpack->Read<ServiceIdT>() ;

			WWRP<SPT> provider = ServiceNodeT::Get(sid);

			if( provider == NULL ) {
				WWSP<Packet> packet( new Packet() );
				packet->Write<wawo::CodeT>( wawo::E_PEER_INVALID_REQUEST );

				byte_t unknown[] = "service not available";
				packet->Write( (byte_t const* const)unknown, wawo::strlen((char const* const)(unknown)) );

				WWSP<MessageT> respm( new MessageT(packet) );
				evt->GetPeer()->Respond( respm, evt->GetIncoming() ) ;
			} else {
				provider->WaitMessage( evt->GetPeer(), evt->GetIncoming());
			}
		}
	};
}}
#endif