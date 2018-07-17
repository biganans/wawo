
#include "./../../../3rd/http_parser/http_parser.h"
#include <wawo/net/protocol/http.hpp>

namespace wawo { namespace net { namespace protocol { namespace http {

	void encode_message(WWSP<message> const& m, WWRP<packet>& out) {
		WWRP<packet> _out = wawo::make_ref<packet>();

		WAWO_ASSERT(m->ver.major != 0);

		if (m->type == T_REQ) {
			if (m->url == "") {
				m->url = "/";
			}

			if (!m->h.have("User-Agent")) {
				m->h.set("User-Agent", "wawo/1.1");
			}

			char resp[512] = { 0 };
			snprintf(resp, sizeof(resp) / sizeof(resp[0]), "%s %s HTTP/%d.%d", protocol::http::option_name[m->opt], m->url.c_str(), m->ver.major, m->ver.minor);
			_out->write((wawo::byte_t*)resp, (wawo::u32_t)wawo::strlen(resp));
			_out->write((wawo::byte_t*)WAWO_HTTP_CRLF, (wawo::u32_t)wawo::strlen(WAWO_HTTP_CRLF));
		} else if (m->type == T_RESP) {

			if ((m->status_code == 200) && m->status == "") {
				m->status = "OK";
			}

			char resp[512] = { 0 };
			WAWO_ASSERT(m->status.length() > 0);

			snprintf(resp, sizeof(resp) / sizeof(resp[0]), "HTTP/%d.%d %d %s", m->ver.major, m->ver.minor, m->status_code, m->status.c_str());
			_out->write((wawo::byte_t*)resp, (wawo::u32_t)wawo::strlen(resp));
			_out->write((wawo::byte_t*)WAWO_HTTP_CRLF, (wawo::u32_t)wawo::strlen(WAWO_HTTP_CRLF));

			if (!m->h.have("Server")) {
				m->h.set("Server", "wawo/1.1");
			}
		}

		if (m->body != NULL && (m->body->len() > 0)) {
			char blength_char[16] = { 0 };
			snprintf(blength_char, 16, "%d", m->body->len());
			m->h.set("Content-Length", blength_char);
		}

		WWRP<packet> hpacket;
		m->h.encode(hpacket);
		_out->write(hpacket->begin(), hpacket->len());

		if (m->body != NULL && (m->body->len() > 0)) {
			_out->write((wawo::byte_t*)m->body->begin(), m->body->len());
		}

		out = _out;
	}

	void message::encode(WWRP<packet>& outp) {
		WWSP<wawo::net::protocol::http::message> m = wawo::make_shared<wawo::net::protocol::http::message>(*this);
		encode_message( m, outp );
	}

	int parse_url(std::string const& url, url_fields& urlfields, bool is_connect ) {

		http_parser_url u;
		int rt = http_parser_parse_url(url.c_str(), url.length(), is_connect, &u);
		WAWO_RETURN_V_IF_NOT_MATCH( WAWO_NEGATIVE(rt), rt == 0);

		if (u.field_set& (1 << UF_SCHEMA)) {
			urlfields.schema = url.substr(u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len);
		}

		if (u.field_set& (1 << UF_HOST)) {
			urlfields.host = url.substr(u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
		}

		if (u.field_set&(1 << UF_PATH)) {
			urlfields.path = url.substr(u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);
		}

		if (u.field_set&(1 << UF_QUERY)) {
			urlfields.query = url.substr(u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len);
		}
		if (u.field_set&(1 << UF_USERINFO)) {
			urlfields.userinfo = url.substr(u.field_data[UF_USERINFO].off, u.field_data[UF_USERINFO].len);
		}
		if (u.field_set&(1 << UF_FRAGMENT)) {
			urlfields.fragment = url.substr(u.field_data[UF_FRAGMENT].off, u.field_data[UF_FRAGMENT].len);
		}

		if (u.field_set&(1 << UF_PORT)) {
			urlfields.port = u.port;
		}
		else {
			urlfields.port = 80;
		}

		return wawo::OK;
	}

	inline static int _on_message_begin(http_parser* p_ ) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		p->type = (u8_t)p_->type;
		
		if (p->on_message_begin != NULL) return p->on_message_begin(WWRP<parser>(p));
		return 0;
	}

	inline static int _on_url(http_parser* p_, const char* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);

		switch (p_->type) {
		case HTTP_REQUEST:
			{
				p->type = T_REQ;
			}
			break;
		case HTTP_RESPONSE:
			{
				p->type = T_RESP;
			}
			break;
		}


		switch (p_->method) {
		case HTTP_DELETE:
			{
				p->opt = protocol::http::O_DELETE;
			}
			break;
		case HTTP_GET:
			{
				p->opt = protocol::http::O_GET;
			}
			break;

		case HTTP_HEAD:
			{
				p->opt = protocol::http::O_HEAD;
			}
			break;
		case HTTP_POST:
			{
				p->opt = protocol::http::O_POST;
			}
			break;
		case HTTP_PUT:
			{
				p->opt = protocol::http::O_PUT;
			}
			break;
		case HTTP_CONNECT:
			{
				p->opt = protocol::http::O_CONNECT;
			}
			break;
		case HTTP_OPTIONS:
			{
				p->opt = protocol::http::O_OPTIONS;
			}
			break;
		case HTTP_TRACE:
			{
				p->opt = protocol::http::O_TRACE;
			}
			break;
		default:
			{
				WAWO_ASSERT(!"UNKNOWN HTTP METHOD");
			}
			break;
		}

		p->url = std::string(data, len);
		if (p->on_url) return p->on_url(WWRP<parser>(p),data, (u32_t)len);

		return 0;
	}

	inline static int _on_status(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);

		WAWO_ASSERT(p_->status_code != 0);
		p->status_code = p_->status_code;
		p->ver.major = p_->http_major;
		p->ver.minor = p_->http_minor;

		if (p->on_status) return p->on_status( WWRP<parser>(p), data, (u32_t)len);
		return 0;
	}

	inline static int _on_header_field(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		if (p->on_header_field) return p->on_header_field(WWRP<parser>(p), data, (u32_t)len);
		return 0;
	}

	inline static int _on_header_value(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		if (p->on_header_value) return p->on_header_value(WWRP<parser>(p), data, (u32_t)len);
		return 0;
	}

	inline static int _on_headers_complete(http_parser* p_) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);

		p->ver.major = p_->http_major;
		p->ver.minor = p_->http_minor;

		if (p->on_headers_complete) return p->on_headers_complete(WWRP<parser>(p));
		return 0;
	}

	inline static int _on_body(http_parser* p_, char const* data, ::size_t len) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		if (p->on_body) return p->on_body(WWRP<parser>(p), data, (u32_t)len);
		return 0;
	}

	inline static int _on_message_complete(http_parser* p_ ) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);

		if (p->on_message_complete) return p->on_message_complete(WWRP<parser>(p));
		return 0;
	}

	inline static int _on_chunk_header(http_parser* p_) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		if (p->on_chunk_header) return p->on_chunk_header(WWRP<parser>(p));
		return 0;
	}

	inline static int _on_chunk_complete(http_parser* p_) {
		parser* p = (parser*)p_->data;
		WAWO_ASSERT(p != NULL);
		if (p->on_chunk_complete) return p->on_chunk_complete(WWRP<parser>(p));
		return 0;
	}

	parser::parser():
		_p(NULL),
		on_message_begin(NULL),
		on_url(NULL),
		on_header_field(NULL),
		on_header_value(NULL),
		on_headers_complete(NULL),
		on_body(NULL),
		on_message_complete(NULL),
		on_chunk_header(NULL),
		on_chunk_complete(NULL)
	{
	}

	parser::~parser() {
		deinit();
	}

	void parser::init(parser_type const& type_ = PARSER_REQ ) {
		WAWO_ASSERT(_p == NULL);
		_p = (http_parser*) ::malloc(sizeof(http_parser));
		WAWO_ALLOC_CHECK(_p, sizeof(http_parser));

		if (type_ == PARSER_REQ) {
			http_parser_init(_p, HTTP_REQUEST);
		}
		else {
			http_parser_init(_p, HTTP_RESPONSE);
		}

		_p->data = this;
	}

	void parser::deinit() {
		if (_p != NULL) {
			::free(_p);
			_p = NULL;
		}
//		ctx = NULL;

		reset();
	}

	void parser::reset() {
		on_message_begin = NULL;
		on_url = NULL;

		on_status = NULL;
		on_header_field = NULL;
		on_header_value = NULL;
		on_headers_complete = NULL;
		on_body = NULL;
		on_message_complete = NULL;

		on_chunk_header = NULL;
		on_chunk_complete = NULL;
	}

	//return number of parsed bytes 
	wawo::u32_t parser::parse(char const* const data, wawo::u32_t const& len, int& ec) {
		WAWO_ASSERT(_p != NULL);

		static http_parser_settings _settings;
		_settings.on_message_begin = _on_message_begin;
		_settings.on_url = _on_url;
		_settings.on_status = _on_status;
		_settings.on_header_field = _on_header_field;
		_settings.on_header_value = _on_header_value;
		_settings.on_headers_complete = _on_headers_complete;
		_settings.on_body = _on_body;
		_settings.on_message_complete = _on_message_complete;

		_settings.on_chunk_header = _on_chunk_header;
		_settings.on_chunk_complete = _on_chunk_complete;

		::size_t nparsed = http_parser_execute(_p, &_settings, data, len);

		ec = _p->http_errno;
		return (u32_t)nparsed;
	}

}}}}