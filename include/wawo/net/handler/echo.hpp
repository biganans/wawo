#ifndef _WAWO_NET_HANDLER_ECHO_HPP
#define _WAWO_NET_HANDLER_ECHO_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_handler.hpp>

namespace wawo {namespace net {namespace handler {

	class echo:
	public wawo::net::channel_inbound_handler_abstract
{
public:
	void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<wawo::packet> const& income)
	{	
		/*
		WWSP<wawo::net::protocol::http::message> m = wawo::make_shared<wawo::net::protocol::http::message>();
		m->ver = { 1,1 };
		m->status_code = 200;
		m->status = "ok";
		m->h.set("server", "wawo/1.0");
		m->body = "zzzz";

		WWSP<wawo::packet> outp = wawo::make_shared<wawo::packet>();
		wawo::net::protocol::http::encode_message(m, outp);
		*/

		ctx->write(income);
		ctx->close();
	}
};

}}}
#endif