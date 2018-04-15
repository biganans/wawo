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

	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_message_begin_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_message_body_end_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx)> fn_message_end_t;

	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx, WWSP<protocol::http::message> const& m)> fn_message_header_end_t;
	typedef std::function<void(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& body)> fn_message_body_t;

	class http:
		public wawo::event_trigger,
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_activity_handler_abstract
	{
		WWRP<protocol::http::parser> m_http_parser;
		wawo::len_cstr m_tmp_for_field;

		WWSP<protocol::http::message> m_tmp_m;
		WWRP<wawo::net::channel_handler_context> m_cur_ctx;
	public:
		void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income);

		int http_on_message_begin();
		int http_on_url(const char* data, u32_t const& len);
		int http_on_status(const char* data, u32_t const& len);
		int http_on_header_field(const char* data, u32_t const& len);
		int http_on_header_value(const char* data, u32_t const& len);
		int http_on_headers_complete();
		int http_on_body(const char* data, u32_t const& len);
		int http_on_message_complete();

		int http_on_chunk_header();
		int http_on_chunk_complete();
	};
}}}
#endif