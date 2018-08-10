#ifndef _WAWO_NET_PROTOCOL_HTTP_HPP
#define _WAWO_NET_PROTOCOL_HTTP_HPP

#include <unordered_map>
#include <list>
#include <functional>

#include "./../../../../3rd/http_parser/http_parser.h"

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

#define WAWO_HTTP_CR	"\r"
#define WAWO_HTTP_LF	"\n"
#define WAWO_HTTP_SP	" "
#define WAWO_HTTP_CRLF	"\r\n"
#define WAWO_HTTP_COLON	":"
#define WAWO_HTTP_COLON_SP	": "


#define WAWO_HTTP_METHOD_NAME_MAX_LEN 7

namespace wawo { namespace net { namespace protocol { namespace http {

	enum message_type {
		T_REQ,
		T_RESP
	};

	//http://tools.ietf.org/html/rfc2616#section-9.2
	enum option {
		O_NONE = -1,
		O_GET = 0,
		O_HEAD,
		O_POST,
		O_PUT,
		O_DELETE,
		O_CONNECT,
		O_OPTIONS,
		O_TRACE,
		O_MAX
	};

	const static char* option_name[O_MAX] = {
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"CONNECT",
		"OPTIONS",
		"TRACE"
	};


	struct version {
		u16_t major;
		u16_t minor;
	};

	struct header {

		typedef std::unordered_map<std::string, std::string>	HeaderMap;
		typedef std::pair<std::string, std::string>	HeaderPair;

		HeaderMap map;
		std::list<std::string> keys_order;

		header() {}
		~header() {}

		bool have(std::string const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			return it != map.end();
		}

		void remove(std::string const& field) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				map.erase(it);

				std::list<std::string>::iterator it_key = std::find_if(keys_order.begin(), keys_order.end(), [&field](std::string const& key) {
					return key == field;
				});

				WAWO_ASSERT(it_key != keys_order.end());
				keys_order.erase(it_key);
			}
		}

		std::string get(std::string const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			if (it != map.end()) {
				return it->second;
			}
			return "";
		}

		void set(std::string const& field, std::string const& value) {

			/*
			 * replace if exists, otherwise add to tail
			 */
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				it->second = value;
				return;
			}

			HeaderPair pair(field, value);
			map.insert(pair);

			keys_order.push_back(field);
		}

		void replace(std::string const& field, std::string const& value) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				it->second = value;
			}
		}

		void encode(WWRP<packet>& packet_o) {
			WWRP<packet> _out = wawo::make_ref<packet>();
			std::for_each( keys_order.begin(), keys_order.end(), [&](std::string const& key ) {
				const std::string& value = map[key];
				_out->write((wawo::byte_t*)key.c_str(), (wawo::u32_t)key.length());
				_out->write((wawo::byte_t*)WAWO_HTTP_COLON_SP, 2);
				_out->write((wawo::byte_t*)value.c_str(), (wawo::u32_t)value.length());
				_out->write((wawo::byte_t*)WAWO_HTTP_CRLF, (wawo::u32_t)wawo::strlen(WAWO_HTTP_CRLF));
			});
			_out->write((wawo::byte_t*)WAWO_HTTP_CRLF, (wawo::u32_t)wawo::strlen(WAWO_HTTP_CRLF));
			packet_o = _out;
		}
	};

	struct url_fields {
		std::string schema;
		std::string host;
		std::string path;
		std::string query;
		std::string fragment;
		std::string userinfo;
		u16_t port;
	};

	struct message {
		message_type type;
		option opt;
		version ver;

		u16_t status_code;
		std::string url;
		std::string status;

		header h;
		WWRP<wawo::packet> body;
		url_fields urlfields;

		void encode( WWRP<wawo::packet>& out );
	};

	int parse_url(std::string const& url, url_fields& urlfields, bool is_connect);

	struct parser;

	typedef std::function<int(WWRP<parser> const&, const char* data, wawo::u32_t const& len)> parser_cb_data;
	typedef std::function<int(WWRP<parser> const&)> parser_cb;

	enum parser_type {
		PARSER_REQ,
		PARSER_RESP
	};

	struct parser:
		public wawo::ref_base
	{
		WWRP<wawo::ref_base> ctx;

		http_parser* _p;

		u8_t type;
		option opt;
		version ver;

		u16_t status_code;
		std::string url;

		parser();
		~parser();

		void init(parser_type const& type);
		void deinit();
		void reset();

		wawo::u32_t parse(char const* const data, wawo::u32_t const& len, int& ec);

		parser_cb on_message_begin;
		parser_cb_data on_url;

		parser_cb_data on_status;
		parser_cb_data on_header_field;
		parser_cb_data on_header_value;
		parser_cb on_headers_complete;
		parser_cb_data on_body;
		parser_cb on_message_complete;

		parser_cb on_chunk_header;
		parser_cb on_chunk_complete;
	};

}}}}

#endif