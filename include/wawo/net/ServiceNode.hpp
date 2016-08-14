#ifndef _WAWO_NET_SERVICE_NODE_HPP
#define _WAWO_NET_SERVICE_NODE_HPP

#include <wawo/core.h>
#include <wawo/net/ServicePool.hpp>
#include <wawo/net/ServiceProvider_Abstract.hpp>
#include <wawo/net/Node_Abstract.hpp>

namespace wawo { namespace net {

	template <class _PeerT>
	class ServiceNode:
		public ServicePool< ServiceProvider_Abstract<_PeerT> >,
		public Node_Abstract<_PeerT>
	{

	public:
		typedef _PeerT PeerT;
		typedef typename Node_Abstract<_PeerT>::ListenerT ListenerT;
		typedef typename _PeerT::MessageT MessageT;
		typedef typename _PeerT::PeerEventT PeerEventT;

		typedef ServiceProvider_Abstract<_PeerT> SPT;

	public:
		virtual void OnPeerMessage(WWRP<PeerEventT> const& pevt ) = 0;

		void OnPeerSocketReadShutdown(WWRP<PeerEventT> const& pevt) {
			WAWO_ASSERT( pevt->GetSocket() != NULL );
			pevt->GetSocket()->Close(pevt->GetCookie().int32_v);
		}
		void OnPeerSocketWriteShutdown(WWRP<PeerEventT> const& pevt) {
			WAWO_ASSERT(pevt->GetSocket() != NULL);
			pevt->GetSocket()->Close(pevt->GetCookie().int32_v);
		}
	};
}}
#endif