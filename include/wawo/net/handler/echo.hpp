#ifndef _WAWO_NET_HANDLER_ECHO_HPP
#define _WAWO_NET_HANDLER_ECHO_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_handler.hpp>

namespace wawo {namespace net {namespace handler {

	class echo:
	public wawo::net::channel_inbound_handler_abstract {
public:
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income)
	{	
		ctx->write(income);
	}
};

}}}
#endif