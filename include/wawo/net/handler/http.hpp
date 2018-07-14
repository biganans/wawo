#ifndef _WAWO_NET_HANDLER_HTTP_HPP
#define _WAWO_NET_HANDLER_HTTP_HPP

#include <wawo/event_trigger.hpp>
#include <wawo/net/channel_handler.hpp>

#include <wawo/net/protocol/http.hpp>

#include <functional>
namespace wawo { namespace net { namespace handler {

	enum http_event {
		E_MESSAGE_BEGIN,
		E_HEADER_COMPLETE,
		E_BODY,
		E_BODY_COMPLETE,
		E_MESSAGE_COMPLETE
	};

	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_http_message_begin_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_http_message_body_end_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_http_message_end_t;

	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<protocol::http::message> const& m)> fn_http_message_header_end_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& body)> fn_http_message_body_t;

	class http:
		public wawo::event_trigger,
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract
	{
		WWRP<protocol::http::parser> m_http_parser;
		std::string m_tmp_for_field;

		WWSP<protocol::http::message> m_tmp_m;
		WWRP<wawo::net::channel_handler_context> m_cur_ctx;
	public:
		virtual void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		virtual void closed(WWRP<wawo::net::channel_handler_context> const& ctx);

		virtual void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);

		int http_on_message_begin(WWRP<protocol::http::parser> const& p);
		int http_on_url(WWRP<protocol::http::parser> const& p,const char* data, wawo::u32_t const& len);
		int http_on_status(WWRP<protocol::http::parser> const& p, const char* data, wawo::u32_t const& len);
		int http_on_header_field(WWRP<protocol::http::parser> const& p, const char* data, wawo::u32_t const& len);
		int http_on_header_value(WWRP<protocol::http::parser> const& p, const char* data, wawo::u32_t const& len);
		int http_on_headers_complete(WWRP<protocol::http::parser> const& p);
		int http_on_body(WWRP<protocol::http::parser> const& p, const char* data, wawo::u32_t const& len);
		int http_on_message_complete(WWRP<protocol::http::parser> const& p);

		int http_on_chunk_header(WWRP<protocol::http::parser> const& p);
		int http_on_chunk_complete(WWRP<protocol::http::parser> const& p);
	};
}}}
#endif