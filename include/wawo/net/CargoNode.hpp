#ifndef _WAWO_NET_CARGONODE_HPP
#define _WAWO_NET_CARGONODE_HPP

#include <wawo/net/Node_Abstract.hpp>

namespace wawo { namespace net {

	template <class _PeerT>
	class CargoNode:
		public Node_Abstract<_PeerT>
	{

	public:
		typedef _PeerT PeerT;
		typedef typename PeerT::MessageT MessageT;
		typedef typename PeerT::PeerEventT PeerEventT;

		virtual void OnPeerMessage(WWRP<PeerEventT> const& evt) = 0;
	};
}}
#endif