#ifndef _WAWO_NET_HTTPNODE_HPP
#define _WAWO_NET_HTTPNODE_HPP

#include <wawo/net/peer/http.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::peer;

	class http_node:
		public node_abstract<http>
	{

	public:
		typedef typename node_abstract<http>::peer_event_t peer_event_t;

	public:

		void on_accepted(WWRP<http> const& peer, WWRP<socket> const& so) {
			(void)peer;
			(void)so;

			WAWO_DEBUG("[http_node][%d:%s]local addr: %s, on accepted", so->get_fd(), so->get_addr_info().cstr, so->get_local_addr().address_info().cstr );
		}

		virtual void on_message( WWRP<peer_event_t> const& pevt ) = 0;

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
