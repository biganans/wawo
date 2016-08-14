#ifndef WAWO_NET_PEER_MESSAGE_HTTP_HPP
#define WAWO_NET_PEER_MESSAGE_HTTP_HPP

#include <map>
#include <wawo/core.h>


#define WAWO_HTTP_CR	"\r"
#define WAWO_HTTP_LF	"\n"
#define WAWO_HTTP_SP	" "
#define WAWO_HTTP_CRLF	"\r\n"
#define WAWO_HTTP_COLON	":"

namespace wawo { namespace net { namespace peer { namespace http {

	using namespace wawo::algorithm;

	struct Version {
		u16_t major;
		u16_t minor;
	};

	class Header {
		typedef std::map<Len_CStr, Len_CStr>	HeaderMap;
		typedef std::pair<Len_CStr, Len_CStr>	HeaderPair;
	private:
		HeaderMap map;
	public:
		Header() {}
		~Header() {}

		bool Have(Len_CStr const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			return it != map.end();
		}

		void Remove(Len_CStr const& field) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				map.erase(it);
			}
		}

		Len_CStr Get(Len_CStr const& field) const {
			HeaderMap::const_iterator it = map.find(field);
			if (it != map.end()) {
				return it->second;
			}
			return "";
		}

		void Set(Len_CStr const& field, Len_CStr const& value) {
			HeaderMap::iterator it = map.find(field);
			if (it != map.end()) {
				map.erase(it);
			}
			HeaderPair pair(field, value);
			map.insert(pair);
		}

		void GetAllFields(std::vector<Len_CStr>& fields) const {
			std::for_each(map.begin(), map.end(), [&fields](HeaderPair const& pair) {
				fields.push_back(pair.first);
			});
		}

		int Encode( WWSP<Packet>& packet_o) {
			WWSP<Packet> opacket( new Packet() );

			if (map.size() == 0) {
				return wawo::OK;
			}

			std::for_each(map.begin(), map.end(), [&opacket](HeaderPair const& pair) {
				opacket->Write( (wawo::byte_t*)pair.first.CStr(), pair.first.Len() );
				opacket->Write((wawo::byte_t*)WAWO_HTTP_COLON, 1 );
				opacket->Write((wawo::byte_t*)WAWO_HTTP_SP, 1);
				opacket->Write((wawo::byte_t*)pair.second.CStr(), pair.second.Len());
				opacket->Write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF) );
			});

			opacket->Write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));

			packet_o = opacket;
			return wawo::OK;
		}

		/*
		Len_CStr& operator[] (Len_CStr const& field ) {
		HeaderMap::iterator it = map.find(field);
		if (it == map.end()) {
		HeaderPair pair(field, "" );
		std::pair< HeaderMap::iterator, bool> iret = map.insert(pair);
		WAWO_ASSERT( iret.second == true);
		return iret.first->second;
		}

		return it->second;
		}
		*/
	};

	//http://tools.ietf.org/html/rfc2616#section-9.2
	enum Option {
		O_NONE = -1,
		O_GET = 0,
		O_HEAD,
		O_POST,
		O_PUT,
		O_DELETE,
		O_TRACE,
		O_CONNECT,
		O_MAX
	};

	const static Len_CStr OptionName[O_MAX] = {
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"TRACE",
		"CONNECT"
	};

}}}}

namespace wawo { namespace net { namespace peer { namespace message {

	using namespace wawo::net::peer::http;

	class Http {

	public:
		enum Type {
			T_NONE,
			T_REQUEST,
			T_RESPONSE
		};

	private:
		Type m_type;
		Option m_option;
		Version m_ver;
		Len_CStr m_url;
		Header m_header;
		wawo::Len_CStr m_body;
		int m_code;
		wawo::Len_CStr m_status;

	public:
		Http():
			m_type(T_NONE),
			m_option(O_NONE),
			m_ver()
		{
			m_ver.major = 1;
			m_ver.minor = 1;
		}
		virtual ~Http() {}

		void SetType(Type const& t) {
			m_type = t;
		}

		void SetOption(Option const& o) {
			m_option = o;
		}

		void SetUrl(Len_CStr const& url) {
			m_url = url;
		}
		void SetBody(Len_CStr const& body) {
			m_body = body;
		}

		void AddHeader(Len_CStr const& field, Len_CStr const& value) {
			m_header.Set(field, value);
		}

		void RemoveHeader(Len_CStr const& field) {
			m_header.Remove(field);
		}

		Len_CStr GetHeader(Len_CStr const& field) const {
			return m_header.Get(field);
		}

		void SetVersion(Version const& ver) {
			m_ver = ver;
		}

		void SetStatus(Len_CStr const& status) {
			m_status = status;
		}

		void SetCode(int const& code) {
			m_code = code;
		}

		Type GetType() const { return m_type; }
		Option GetOption() const { return m_option; }

		Len_CStr GetUrl() const { return m_url; }
		Len_CStr GetPath() const;
		Len_CStr GetQueryString() const;
		Len_CStr GetFragment() const;

		Header GetHeader() const {
			return m_header;
		}

		Len_CStr GetBody() const {
			return m_body;
		}

		Version GetVersion() const {
			return m_ver;
		}

		Len_CStr GetStatus() const {
			return m_status;
		}

		int GetCode() const {
			return m_code;
		}

		int Encode(WWSP<Packet>& packet_o ) {

			WWSP<Packet> httppack( new Packet() );

			if (m_type == T_REQUEST) {
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_HTTP_REQUEST_MISSING_OPTION, (m_option < http::O_MAX) && (m_option > http::O_NONE));

				if (m_url == "") {
					m_url = "/";
				}

				if (!m_header.Have("User-Agent")) {
					m_header.Set( "User-Agent", "wawo/1.1" );
				}

				char resp[512] = { 0 };
				snprintf(resp, sizeof(resp) / sizeof(resp[0]), "%s %s HTTP/%d.%d", http::OptionName[m_option].CStr(), m_url.CStr(), m_ver.major, m_ver.minor );
				httppack->Write((wawo::byte_t*)resp, wawo::strlen(resp));
				httppack->Write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));
			} else if (m_type == T_RESPONSE) {

				if ( (m_code == 200) && m_status == "") {
					m_status = "OK";
				}

				char resp[128] = { 0 };
				WAWO_ASSERT( m_status.Len() > 0 );

				snprintf(resp, sizeof(resp)/sizeof(resp[0]), "HTTP/%d.%d %d %s", m_ver.major, m_ver.minor, m_code, m_status.CStr() );
				httppack->Write((wawo::byte_t*)resp, wawo::strlen(resp) );
				httppack->Write((wawo::byte_t*)WAWO_HTTP_CRLF, wawo::strlen(WAWO_HTTP_CRLF));

			} else {
				return wawo::E_HTTP_MESSAGE_INVALID_TYPE;
			}

			if (!m_header.Have("Server")) {
				m_header.Set("Server", "wawo/1.1");
			}

			if (m_body.Len() > 0) {
				char blen_char[16] = { 0 };
				snprintf(blen_char, 16, "%d", m_body.Len() );
				m_header.Set("Content-Length", blen_char);
			}

			WWSP<Packet> hpacket;
			int hencrt = m_header.Encode(hpacket);
			WAWO_RETURN_V_IF_NOT_MATCH(hencrt, (hencrt==wawo::OK));

			httppack->Write( hpacket->Begin(), hpacket->Length() );

			if (m_body.Len() > 0) {
				httppack->Write((wawo::byte_t*)m_body.CStr(), m_body.Len() );
			}

			packet_o = httppack;
			return wawo::OK;
		}

	};
}}}}
#endif