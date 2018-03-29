#ifndef _WAWO_NET_HANDLER_ECHO_HPP
#define _WAWO_NET_HANDLER_ECHO_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/socket_handler.hpp>

#include <wawo/log/logger_manager.h>

namespace wawo {namespace net {namespace handler {

	class echo:
	public wawo::net::socket_inbound_handler_abstract
{
public:
	void read(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& income)
	{
		//WAWO_INFO("<<< %s", wawo::len_cstr( (char*)income->begin(), income->len() ).cstr );
		ctx->write(income);
	}
};

}}}
#endif