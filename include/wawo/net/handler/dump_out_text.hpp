#ifndef _WAWO_NET_HANDLER_DUMP_OUT_TEXT_HPP
#define _WAWO_NET_HANDLER_DUMP_OUT_TEXT_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/socket_handler.hpp>

#include <wawo/log/logger_manager.h>

namespace wawo {namespace net {namespace handler {

	class dump_out_text:
	public wawo::net::socket_outbound_handler_abstract
{
public:
	void write(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& outlet)
	{
		WAWO_INFO(">>> %s", wawo::len_cstr( (char*)outlet->begin(), outlet->len() ).cstr );
		ctx->write(outlet);
	}
};

}}}
#endif