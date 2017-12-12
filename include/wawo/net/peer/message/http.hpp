#ifndef WAWO_NET_PEER_MESSAGE_HTTP_HPP
#define WAWO_NET_PEER_MESSAGE_HTTP_HPP

#include <map>
#include <wawo/core.hpp>
#include <wawo/net/protocol/http.hpp>

namespace wawo { namespace net { namespace peer { namespace message {

	struct http {

	public:
		enum http_type {
			T_NONE,
			T_REQUEST,
			T_RESPONSE
		};

		http_type type;
		protocol::http::option option;
		protocol::http::version version;
		len_cstr url;
		protocol::http::header header;
		wawo::len_cstr body;
		int code;
		wawo::len_cstr status;

		http():
			type(T_NONE),
			option(protocol::http::O_NONE),
			version()
		{
			version.major = 1;
			version.minor = 1;
		}

		~http() {}

		void add_header_line(len_cstr const& field, len_cstr const& value) {
			header.set(field, value);
		}

		void remove_header(len_cstr const& field) {
			header.remove(field);
		}

		len_cstr get_header(len_cstr const& field) const {
			return header.get(field);
		}

		len_cstr get_path() const;
		len_cstr get_query_string() const;
		len_cstr get_fragment() const;

		int encode(WWSP<packet>& packet_o ) {

			WWSP<packet> httppack = wawo::make_shared<packet>();

			if ( type == T_REQUEST) {
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_HTTP_REQUEST_MISSING_OPTION, (option < protocol::http::O_MAX) && (option > protocol::http::O_NONE));

				if (url == "") {
					url = "/";
				}

				if (!header.have("User-Agent")) {
					header.set( "User-Agent", "wawo/1.1" );
				}

				char resp[512] = { 0 };
				snprintf(resp, sizeof(resp) / sizeof(resp[0]), "%s %s HTTP/%d.%d", protocol::http::option_name[option], url.cstr, version.major, version.minor );
				httppack->write((wawo::byte_t*)resp, wawo::strlen(resp));
				httppack->write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));
			} else if (type == T_RESPONSE) {

				if ( (code == 200) && status == "") {
					status = "OK";
				}

				char resp[128] = { 0 };
				WAWO_ASSERT( status.len > 0 );

				snprintf(resp, sizeof(resp)/sizeof(resp[0]), "HTTP/%d.%d %d %s", version.major, version.minor, code, status.cstr );
				httppack->write((wawo::byte_t*)resp, wawo::strlen(resp) );
				httppack->write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));

			} else {
				return wawo::E_HTTP_MESSAGE_INVALID_TYPE;
			}

			if (!header.have("Server")) {
				header.set("Server", "wawo/1.1");
			}

			if (body.len > 0) {
				char blength_char[16] = { 0 };
				snprintf(blength_char, 16, "%d", body.len );
				header.set("content-length", blength_char);
			}

			WWSP<packet> hpacket;
			int hencrt = header.encode(hpacket);
			WAWO_RETURN_V_IF_NOT_MATCH(hencrt, (hencrt==wawo::OK));

			httppack->write( hpacket->begin(), hpacket->len() );

			if (body.len > 0) {
				httppack->write((wawo::byte_t*)body.cstr, body.len );
			}

			packet_o = httppack;
			return wawo::OK;
		}

	};
}}}}
#endif
