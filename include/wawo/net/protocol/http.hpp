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

		typedef std::unordered_map<len_cstr, len_cstr>	HeaderMap;
		typedef std::pair<len_cstr, len_cstr>	HeaderPair;

		HeaderMap map;
		std::list<len_cstr> keys_order;

		header() {}
		~header() {}

		bool have(len_cstr const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			return it != map.end();
		}

		void remove(len_cstr const& field) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				map.erase(it);

				std::list<len_cstr>::iterator it_key = std::find_if(keys_order.begin(), keys_order.end(), [&field](len_cstr const& key) {
					return key == field;
				});

				WAWO_ASSERT(it_key != keys_order.end());
				keys_order.erase(it_key);
			}
		}

		len_cstr get(len_cstr const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			if (it != map.end()) {
				return it->second;
			}
			return "";
		}

		void set(len_cstr const& field, len_cstr const& value) {

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

		void replace(len_cstr const& field, len_cstr const& value) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				it->second = value;
			}
		}

		void encode(WWRP<packet>& packet_o) {
			WWRP<packet> opacket = wawo::make_ref<packet>();

			//WAWO_ASSERT(map.size() > 0);

			std::for_each( keys_order.begin(), keys_order.end(), [&]( len_cstr const& key ) {
				len_cstr value = map[key];
				opacket->write((wawo::byte_t*)key.cstr, key.len);
				opacket->write((wawo::byte_t*)WAWO_HTTP_COLON, 1);
				opacket->write((wawo::byte_t*)WAWO_HTTP_SP, 1);
				opacket->write((wawo::byte_t*)value.cstr, value.len);
				opacket->write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));
			});

			opacket->write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));
			packet_o = opacket;
		}
	};

	struct url_fields {
		wawo::len_cstr schema;
		wawo::len_cstr host;
		wawo::len_cstr path;
		wawo::len_cstr query;
		wawo::len_cstr fragment;
		wawo::len_cstr userinfo;
		u16_t port;
	};

	struct message {
		message_type type;
		option opt;
		version ver;

		u16_t status_code;
		wawo::len_cstr url;
		wawo::len_cstr status;

		header h;
		wawo::len_cstr body;

		url_fields urlfields;
		bool is_header_contain_connection_close;

		void encode( WWRP<wawo::packet>& out );
	};

	int parse_url(wawo::len_cstr const& url, url_fields& urlfields, bool is_connect);

	struct parser;

	typedef std::function<int(const char* data, u32_t const& len)> parser_cb_data;
	typedef std::function<int()> parser_cb;

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
		wawo::len_cstr url;

		parser();
		~parser();

		void init(parser_type const& type);
		void deinit();

		u32_t parse(char const* const data, u32_t const& len, int& ec);

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