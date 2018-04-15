#ifndef _WAWO_NET_HANDLER_HTTP_HANDLER_HPP
#define _WAWO_NET_HANDLER_HTTP_HANDLER_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>

#include <wawo/net/protocol/http.hpp>

namespace wawo { namespace net { namespace handler {
	class http_handler :
		public wawo::net::channel_activity_handler_abstract,
		public wawo::net::channel_inbound_handler_abstract
	{
		WWRP<wawo::net::protocol::http::parser> m_http_parser;

	public:
		void connected(WWRP<wawo::net::channel_handler_context> const& ctx);
		void closed(WWRP<wawo::net::channel_handler_context> const& ctx);

		void read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::net::channel> const& ch);

		int http_on_message_begin();
		int http_on_url(const char* data, wawo::u32_t const& len);
		int http_on_status(const char* data, wawo::u32_t const& len);
		int http_on_header_field(const char* data, wawo::u32_t const& len);
		int http_on_header_value(const char* data, wawo::u32_t const& len);
		int http_on_headers_complete();
		int http_on_body(const char* data, wawo::u32_t const& len);
		int http_on_message_complete();

		int http_on_chunk_header();
		int http_on_chunk_complete();
	};

}}}
#endif