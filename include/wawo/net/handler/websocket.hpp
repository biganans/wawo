#ifndef _WAWO_NET_HANDLER_WEBSOCKET_HPP
#define _WAWO_NET_HANDLER_WEBSOCKET_HPP

#include <wawo/core.hpp>

#ifdef ENABLE_WEBSOCKET

#include <wawo/net/socket_handler.hpp>
#include <wawo/net/protocol/http.hpp>

namespace wawo { namespace net { namespace handler {

	class websocket :
		public wawo::net::socket_inbound_handler_abstract
	{
		enum state {
			S_WAIT_CLIENT_HANDSHAKE_REQ,
			S_HANDSHAKING,
			S_UPGRADE_REQ_MESSAGE_DONE,
			S_OPEN
		};

		wawo::len_cstr m_tmp_for_field;
		WWSP<protocol::http::message> m_upgrade_req;
		state m_state;
		WWRP<wawo::net::protocol::http::parser> m_http_parser;

		public:
			websocket() :
				m_state(S_WAIT_CLIENT_HANDSHAKE_REQ),
				m_http_parser(NULL)
			{}

			virtual ~websocket() {}
			void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) ;

		protected:
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

#endif //endfor ENABLE_WEBSOCKET
#endif