#ifndef _WAWO_NET_HTTPNODE_HPP
#define _WAWO_NET_HTTPNODE_HPP

#include <wawo/net/peer/Http.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::peer;

	class HttpNode:
		public Node_Abstract<Http>
	{

	public:
		typedef Node_Abstract<Http> NodeT;
		typedef NodeT::PeerEventT PeerEventT;

	public:
		virtual void OnPeerMessage( WWRP<PeerEventT> const& pevt ) = 0;

		//{
			//@to be defined by your self

			//impl example
			//check Url info, then forward to relevant service provider
			//possible keyinfo
			//1, prefix of the uri, "*.php *.html"
			//2, host name
	//	}
	};
}}
#endif
