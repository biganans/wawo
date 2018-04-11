#ifndef _WAWO_NET_HANDLER_MUX_HPP
#define _WAWO_NET_HADNLER_MUX_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>

namespace wawo { namespace net { namespace handler {

	class mux :
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_outbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract
	{
	public:




	};
}}}

#endif