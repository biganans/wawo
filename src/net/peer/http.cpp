#include <wawo/net/peer/http.hpp>
#include "./../../../3rd/http_parser/http_parser.h"

inline static int on_message_begin(http_parser* parser);
inline static int on_message_complete(http_parser* parser);
inline static int on_chunk_header(http_parser* parser);
inline static int on_chunk_complete(http_parser* parser);
inline static int on_status(http_parser* parser, char const* data, size_t len);
inline static int on_url(http_parser* parser, char const* data, size_t len);
inline static int on_body(http_parser* parser, char const* data, size_t len);
inline static int on_header_field(http_parser* parser, char const* data, size_t len);
inline static int on_header_value(http_parser* parser, char const* data, size_t len);
inline static int on_headers_complete(http_parser* parser);


namespace wawo { namespace net { namespace peer {

	void http::_alloc_http_parser() {
		WAWO_ASSERT(m_hparser == NULL);
		m_hparser = (http_parser*)::malloc(sizeof(http_parser));
		WAWO_ALLOC_CHECK(m_hparser, sizeof(http_parser));
		http_parser_init(m_hparser, HTTP_BOTH);
	}

	void http::_dealloc_http_parser() {
		free(m_hparser);
		m_hparser = NULL;
	}


	//BLOCK CONNECT
	int http::connect(wawo::len_cstr const& hosturl) {
		if (hosturl.len == 0) {
			return wawo::E_HTTP_MISSING_HOST;
		}

		http_parser_url u;
		memset(&u, 0, sizeof(u));
		int prt = http_parser_parse_url(hosturl.cstr, hosturl.len, false, &u);
		WAWO_RETURN_V_IF_NOT_MATCH(prt, prt == 0);

		wawo::len_cstr schema = hosturl.substr(u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len );
		wawo::len_cstr host = hosturl.substr(u.field_data[UF_HOST].off, u.field_data[UF_HOST].len );

		if (host.len == 0) {
			return wawo::E_HTTP_MISSING_HOST;
		}

		wawo::len_cstr ip;
		int filter = AIF_F_INET | AIF_ST_STREAM;
		wawo::net::get_one_ipaddr_by_host(host.cstr, ip, filter);

		i16_t port;
		if (u.field_data[UF_PORT].off == 0) {
			port = 80;
		}
		else {
			wawo::len_cstr port_cstr = hosturl.substr(u.field_data[UF_PORT].off, u.field_data[UF_PORT].len );
			port = wawo::to_i32(port_cstr.cstr) & 0xFFFF;
		}

		address addr(ip.cstr, port);
		WWRP<socket> bsocket = wawo::make_ref<socket>(wawo::net::F_AF_INET, wawo::net::ST_STREAM, wawo::net::P_TCP);

		int soconnrt = bsocket->connect(addr);
		WAWO_RETURN_V_IF_NOT_MATCH(soconnrt, soconnrt == wawo::OK);

		return wawo::OK;
	}

	int http::http_parser_on_message_complete() {

		WAWO_ASSERT(m_hparser != NULL);
		m_tmp->body = m_tmp_body;

		switch (m_hparser->type) {
		case HTTP_REQUEST:
		{
			static protocol::http::option option_map[] =
			{
				protocol::http::option::O_DELETE,
				protocol::http::option::O_GET,
				protocol::http::option::O_HEAD,
				protocol::http::option::O_POST,
				protocol::http::option::O_PUT,
				protocol::http::option::O_CONNECT
			};

			/*
			XX(0, DELETE, DELETE)       \
			XX(1, GET, GET)          \
			XX(2, HEAD, HEAD)         \
			XX(3, POST, POST)         \
			XX(4, PUT, PUT)          \
			XX(5, CONNECT, CONNECT)      \
			*/

			m_tmp->type = message::http::T_REQUEST;
			m_tmp->option = option_map[m_hparser->method];

			WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(E_MESSAGE, WWRP<self_t>(this), m_so, m_tmp);
			dispatcher_t::trigger(pevt);
		}
		break;
		case HTTP_RESPONSE:
		{
			protocol::http::version ver = { m_hparser->http_major, m_hparser->http_minor };
			m_tmp->type = message::http::T_RESPONSE;
			m_tmp->version = ver;
			m_tmp->code = m_hparser->status_code;

			requested_message reqm;
			{
				lock_guard<spin_mutex> lg(m_requested_mutex);
				WAWO_ASSERT(!m_requested.empty());
				reqm = m_requested.front();
				m_requested.pop();
			}

			WAWO_ASSERT(reqm.cb != NULL);
			reqm.cb->on_respond(m_tmp);
			m_tmp = NULL;
		}
		break;
		default:
		{
			char errstr[512] = { 0 };
			snprintf(errstr, sizeof(errstr) / sizeof(errstr[0]), "[#%d:%s]invalid http message type, unknown: %d", m_so->get_fd(), m_so->get_remote_addr().address_info().cstr, m_hparser->type);
			WAWO_THROW(errstr);
		}
		break;
		}

		m_tmp = NULL;
		http_parser_init(m_hparser, HTTP_BOTH);
		return 0;
	}


	void http::on_event(WWRP<socket_event> const& evt) {
		WAWO_ASSERT(evt->so == m_so);
		u32_t const& id = evt->id;

		switch (id) {
		case E_PACKET_ARRIVE:
		{
			WWSP<packet> const& inpack = evt->data;
			WAWO_ASSERT(inpack != NULL);
			WAWO_ASSERT(inpack->len()>0);

			WAWO_ASSERT(m_hparser != NULL);
			static http_parser_settings settings;

			settings.on_message_begin = on_message_begin;
			settings.on_message_complete = ::on_message_complete;
			settings.on_status = on_status;
			settings.on_body = on_body;
			settings.on_url = on_url;
			settings.on_header_field = on_header_field;
			settings.on_header_value = on_header_value;
			settings.on_headers_complete = on_headers_complete;
			settings.on_chunk_header = on_chunk_header;
			settings.on_chunk_complete = on_chunk_complete;

			WWRP<self_t> __HOLD_MYSELF__(this);
			m_hparser->data = this;
			size_t nparsered = http_parser_execute(m_hparser, &settings, (char*)inpack->begin(), inpack->len());
			m_hparser->data = NULL;

			if (m_hparser->http_errno != wawo::OK) {
				WAWO_ASSERT(evt->so != NULL);
				evt->so->close(m_hparser->http_errno);
			}

			(void)nparsered;
		}
		break;
		case E_RD_SHUTDOWN:
		case E_WR_SHUTDOWN:
		case E_WR_BLOCK:
		case E_WR_UNBLOCK:
		case E_CLOSE:
		case E_ERROR:
		{
			WWRP<peer_event_t> pevt = wawo::make_ref<peer_event_t>(evt->id, WWRP<self_t>(this), evt->so, evt->info );
			dispatcher_t::trigger(pevt);
		}
		break;
		default:
		{
			char tmp[256] = { 0 };
			snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown socket evt: %d", id);
			WAWO_THROW(tmp);
		}
		break;
		}
	}

	int http::get(WWSP<message::http> const& message, WWRP<wawo::net::peer::http_callback_abstract> const& cb, u32_t const& timeout ) {
		WAWO_ASSERT(message->type == message::http::T_REQUEST);
		WAWO_ASSERT(message->option == protocol::http::option::O_NONE);
		WAWO_ASSERT(cb != 0);

		message->option = protocol::http::option::O_GET;

		//http pipeline not supported right now
		lock_guard<spin_mutex> lg(m_requested_mutex);

		if (!m_requested.empty()) {
			return wawo::E_HTTP_REQUEST_NOT_DONE;
		}
		int msndrt = self_t::do_send_message(message);
		WAWO_RETURN_V_IF_NOT_MATCH(msndrt, msndrt == wawo::OK);

		requested_message reqm =
		{
			message,
			cb,
			wawo::time::curr_milliseconds(),
			timeout
		};
		m_requested.push(reqm);
		return msndrt;
	}

	int http::post(WWSP<message::http> const& message, WWRP<wawo::net::peer::http_callback_abstract> const& cb, u32_t const& timeout ) {
		WAWO_ASSERT(message->type == message::http::T_REQUEST);
		WAWO_ASSERT(message->option == protocol::http::option::O_NONE);
		WAWO_ASSERT(cb != 0);

		message->option = protocol::http::option::O_POST;

		//http pipeline not supported right now
		lock_guard<spin_mutex> lg(m_requested_mutex);
		if (!m_requested.empty()) {
			return wawo::E_HTTP_REQUEST_NOT_DONE;
		}

		int msndrt = self_t::do_send_message(message);
		WAWO_RETURN_V_IF_NOT_MATCH(msndrt, msndrt == wawo::OK);

		requested_message reqm =
		{
			message,
			cb,
			wawo::time::curr_milliseconds(),
			timeout
		};
		m_requested.push(reqm);
		return msndrt;
	}
}}}

inline static int on_message_begin(http_parser* parser) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_message_begin();
	return 0;
}
inline static int on_message_complete(http_parser* parser) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_message_complete();
	return 0;
}

inline static int on_chunk_header(http_parser* parser) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_chunk_header();
	return 0;
}

inline static int on_chunk_complete(http_parser* parser) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_chunk_complete();
	return 0;
}

inline static int on_status(http_parser* parser, char const* data, size_t len) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_status(data, len);
	return 0;
}
inline static int on_url(http_parser* parser, char const* data, size_t len) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_url(data, len);
	return 0;
}
inline static int on_body(http_parser* parser, char const* data, size_t len) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_body(data, len);
	return 0;
}

inline static int on_header_field(http_parser* parser, char const* data, size_t len) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_header_field(data, len);
	return 0;
}

inline static int on_header_value(http_parser* parser, char const* data, size_t len) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_header_value(data, len);
	return 0;
}

inline static int on_headers_complete(http_parser* parser) {
	wawo::net::peer::http* http = (wawo::net::peer::http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->http_parser_on_header_complete();
	return 0;
}
