#ifndef _WAWO_NET_HANDLER_DUMP_IN_LEN_HPP
#define _WAWO_NET_HANDLER_DUMP_IN_LEN_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_handler.hpp>

#include <wawo/log/logger_manager.h>

namespace wawo {namespace net {namespace handler {

	class dump_in_len:
	public wawo::net::channel_inbound_handler_abstract
{
public:
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income)
	{
//		WAWO_INFO("<<< len: %u", income->len() );
		ctx->fire_read(income);
	}
};

}}}
#endif