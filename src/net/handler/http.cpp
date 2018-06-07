
#include <wawo/net/handler/http.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net { namespace handler {

	void http::connected(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_ASSERT(m_http_parser == NULL);
		m_http_parser = wawo::make_ref<wawo::net::protocol::http::parser>();
		m_http_parser->init(wawo::net::protocol::http::PARSER_REQ);

		m_http_parser->on_message_begin = std::bind(&http::http_on_message_begin, WWRP<http>(this), std::placeholders::_1);
		m_http_parser->on_url = std::bind(&http::http_on_url, WWRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_http_parser->on_status = std::bind(&http::http_on_status, WWRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_http_parser->on_header_field = std::bind(&http::http_on_header_field, WWRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_http_parser->on_header_value = std::bind(&http::http_on_header_value, WWRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_http_parser->on_headers_complete = std::bind(&http::http_on_headers_complete, WWRP<http>(this), std::placeholders::_1);

		m_http_parser->on_body = std::bind(&http::http_on_body, WWRP<http>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_http_parser->on_message_complete = std::bind(&http::http_on_message_complete, WWRP<http>(this), std::placeholders::_1);

		m_http_parser->on_chunk_header = std::bind(&http::http_on_message_begin, WWRP<http>(this), std::placeholders::_1);
		m_http_parser->on_chunk_complete = std::bind(&http::http_on_message_begin, WWRP<http>(this), std::placeholders::_1);

		ctx->fire_connected();
	}

	void http::closed(WWRP<wawo::net::channel_handler_context> const& ctx) {
		WAWO_ASSERT(m_http_parser != NULL);
		m_http_parser->reset() ;
		ctx->fire_closed();
	}

	void http::read(WWRP<wawo::net::channel_handler_context> const& ctx, WWRP<wawo::packet> const& income) {
		WAWO_ASSERT(m_http_parser != NULL );
		WAWO_ASSERT(income != NULL);

		int ec;
		m_cur_ctx = ctx;
		u32_t nparsed = m_http_parser->parse( (char*) income->begin(), income->len(), ec );
		WAWO_ASSERT(ec == wawo::OK ? nparsed == income->len(): true);

		income->skip(nparsed);
		m_cur_ctx = NULL;

		//ctx->fire_read(income);
	}

	int http::http_on_message_begin(WWRP<protocol::http::parser> const& p) {
		(void)p;
		WAWO_ASSERT(m_tmp_m == NULL );
		m_tmp_m = wawo::make_shared<protocol::http::message>();

		event_trigger::invoke<fn_http_message_begin_t>(E_MESSAGE_BEGIN, m_cur_ctx);
		return wawo::OK;
	}

	int http::http_on_url(WWRP<protocol::http::parser> const& p, const char* data, u32_t const& len) {
		(void)p;
		m_tmp_m->url = wawo::len_cstr( data,len );
		return wawo::OK;
	}

	int http::http_on_status(WWRP<protocol::http::parser> const& p, const char* data, u32_t const& len) {
		(void)p;
		WAWO_ERR("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		WAWO_ASSERT(!"WHAT");
		return wawo::OK;
	}

	int http::http_on_header_field(WWRP<protocol::http::parser> const& p, const char* data, u32_t const& len) {
		(void)p;
		//WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		m_tmp_for_field = wawo::len_cstr(data, len);
		return wawo::OK;
	}

	int http::http_on_header_value(WWRP<protocol::http::parser> const& p, const char* data, u32_t const& len) {
		(void)p;
		//WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		m_tmp_m->h.set(m_tmp_for_field, wawo::len_cstr(data, len));
		return wawo::OK;
	}

	int http::http_on_headers_complete(WWRP<protocol::http::parser> const& p) {
		//WAWO_DEBUG(__FUNCTION__);
		(void)p;
		event_trigger::invoke<fn_http_message_header_end_t>(E_HEADER_COMPLETE, m_cur_ctx, m_tmp_m);
		m_tmp_m = NULL;
		return wawo::OK;
	}

	int http::http_on_body(WWRP<protocol::http::parser> const& p, const char* data, u32_t const& len) {
		WWRP<wawo::packet> income = wawo::make_ref<wawo::packet>((wawo::byte_t*)data, len);
		event_trigger::invoke<fn_http_message_body_t>(E_BODY, m_cur_ctx, income);
		(void)p;
		return wawo::OK;
	}

	int http::http_on_message_complete(WWRP<protocol::http::parser> const& p) {
		event_trigger::invoke<fn_http_message_end_t>(E_BODY, m_cur_ctx);
		(void)p;
		return wawo::OK;
	}

	int http::http_on_chunk_header(WWRP<protocol::http::parser> const& p ) {
		WAWO_ASSERT(!"TODO");
		(void)p;
		return wawo::OK;
	}

	int http::http_on_chunk_complete(WWRP<protocol::http::parser> const& p) {
		WAWO_ASSERT(!"TODO");
		(void)p;
		return wawo::OK;
	}
}}}