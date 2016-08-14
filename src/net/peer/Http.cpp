#include <wawo/net/peer/Http.hpp>
#include "./../../../3rd/http_parser/http_parser.h"

inline static int on_message_begin(http_parser* parser);
inline static int on_message_complete(http_parser* parser);
inline static int on_chunk_header(http_parser* parser);
inline static int on_chunk_complete(http_parser* parser);
inline static int on_status(http_parser* parser, char const* data, size_t length);
inline static int on_url(http_parser* parser, char const* data, size_t length);
inline static int on_body(http_parser* parser, char const* data, size_t length);
inline static int on_header_field(http_parser* parser, char const* data, size_t length);
inline static int on_header_value(http_parser* parser, char const* data, size_t length);
inline static int on_headers_complete(http_parser* parser);


namespace wawo { namespace net { namespace peer {

	void Http::_AllocHttpParser() {
		WAWO_ASSERT(m_hparser == NULL);
		m_hparser = (http_parser*)malloc(sizeof(http_parser));
		WAWO_NULL_POINT_CHECK(m_hparser);
		http_parser_init(m_hparser, HTTP_BOTH);
	}

	void Http::_DeallocHttpParser() {
		free(m_hparser);
		m_hparser = NULL;
	}


	//BLOCK CONNECT
	int Http::Connect(wawo::Len_CStr const& hosturl) {
		if (hosturl.Len() == 0) {
			return wawo::E_HTTP_MISSING_HOST;
		}

		http_parser_url u;
		memset(&u, 0, sizeof(u));
		int prt = http_parser_parse_url(hosturl.CStr(), hosturl.Len(), false, &u);
		WAWO_RETURN_V_IF_NOT_MATCH(prt, prt == 0);

		wawo::Len_CStr schema = hosturl.Substr(u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len);
		wawo::Len_CStr host = hosturl.Substr(u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);

		if (host.Len() == 0) {
			return wawo::E_HTTP_MISSING_HOST;
		}

		wawo::Len_CStr ip;
		int filter = AIF_F_INET | AIF_ST_STREAM;
		wawo::net::GetOneIpAddrByHost(host.CStr(), ip, filter);

		i16_t port;
		if (u.field_data[UF_PORT].off == 0) {
			port = 80;
		}
		else {
			wawo::Len_CStr port_cstr = hosturl.Substr(u.field_data[UF_PORT].off, u.field_data[UF_PORT].len);
			port = wawo::to_i32(port_cstr.CStr()) & 0xFFFF;
		}

		SocketAddr addr(ip.CStr(), port);
		WWRP<Socket> bsocket(new Socket(addr, wawo::net::F_AF_INET, wawo::net::ST_STREAM, wawo::net::P_TCP));

		int soconnrt = bsocket->Connect();
		WAWO_RETURN_V_IF_NOT_MATCH(soconnrt, soconnrt == wawo::OK);

		return wawo::OK;
	}

	int Http::OnMessageComplete() {

		WAWO_ASSERT(m_hparser != NULL);
		m_tmp->SetBody(m_tmp_body);

		switch (m_hparser->type) {
		case HTTP_REQUEST:
		{
			static peer::http::Option option_map[] =
			{
				peer::http::O_DELETE,
				peer::http::O_GET,
				peer::http::O_HEAD,
				peer::http::O_POST,
				peer::http::O_PUT,
				peer::http::O_CONNECT
			};

			/*
			XX(0, DELETE, DELETE)       \
			XX(1, GET, GET)          \
			XX(2, HEAD, HEAD)         \
			XX(3, POST, POST)         \
			XX(4, PUT, PUT)          \
			XX(5, CONNECT, CONNECT)      \
			*/

			m_tmp->SetType(message::Http::T_REQUEST);
			m_tmp->SetOption(option_map[m_hparser->method]);

			WWRP<PeerEventT> pevt(new PeerEventT(PE_MESSAGE, WWRP<MyT>(this), m_socket, m_tmp));
			DispatcherT::Trigger(pevt);
		}
		break;
		case HTTP_RESPONSE:
		{
			http::Version ver = { m_hparser->http_major, m_hparser->http_minor };
			m_tmp->SetType(message::Http::T_RESPONSE);
			m_tmp->SetVersion(ver);
			m_tmp->SetCode(m_hparser->status_code);

			RequestedMessage reqm;
			{
				LockGuard<SpinMutex> lg(m_requested_mutex);
				WAWO_ASSERT(!m_requested.empty());
				reqm = m_requested.front();
				m_requested.pop();
			}

			WAWO_ASSERT(reqm.cb != NULL);
			reqm.cb->OnRespond(m_tmp);
			m_tmp = NULL;
		}
		break;
		default:
		{
			char errstr[512] = { 0 };
			snprintf(errstr, sizeof(errstr) / sizeof(errstr[0]), "[#%d:%s]invalid http message type, unknown: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), m_hparser->type);
			WAWO_THROW_EXCEPTION(errstr);
		}
		break;
		}

		m_tmp = NULL;
		http_parser_init(m_hparser, HTTP_BOTH);
		return 0;
	}


	void Http::OnEvent(WWRP<SocketEvent> const& evt) {
		WAWO_ASSERT(evt->GetSocket() == m_socket);
		u32_t const& id = evt->GetId();
		i32_t  const& ec = evt->GetCookie().int32_v;

		switch (id) {
		case SE_PACKET_ARRIVE:
		{
			WWSP<Packet> const& inpack = evt->GetPacket();
			WAWO_ASSERT(inpack != NULL);
			WAWO_ASSERT(inpack->Length()>0);

			WAWO_ASSERT(m_hparser != NULL);
			static http_parser_settings settings;

			settings.on_message_begin = on_message_begin;
			settings.on_message_complete = on_message_complete;
			settings.on_status = on_status;
			settings.on_body = on_body;
			settings.on_url = on_url;
			settings.on_header_field = on_header_field;
			settings.on_header_value = on_header_value;
			settings.on_headers_complete = on_headers_complete;
			settings.on_chunk_header = on_chunk_header;
			settings.on_chunk_complete = on_chunk_complete;

			WWRP<MyT> __HOLD_MYSELF__(this);
			m_hparser->data = this;
			int packet_parse_ec = wawo::OK;
			size_t nparsered = http_parser_execute(m_hparser, &settings, (char*)inpack->Begin(), inpack->Length());
			WAWO_ASSERT(nparsered == inpack->Length());
			m_hparser->data = NULL;

			if (packet_parse_ec != wawo::OK) {
				WAWO_ASSERT(evt->GetSocket() != NULL);
				evt->GetSocket()->Close(packet_parse_ec);
			}

			(void)nparsered;
		}
		break;
		case SE_RD_SHUTDOWN:
		{
			WWRP<PeerEventT> pevt(new PeerEventT(PE_SOCKET_RD_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket(), evt->GetCookie().int32_v));
			DispatcherT::Trigger(pevt);
		}
		break;
		case SE_WR_SHUTDOWN:
		{
			WWRP<PeerEventT> pevt(new PeerEventT(PE_SOCKET_WR_SHUTDOWN, WWRP<MyT>(this), evt->GetSocket(), evt->GetCookie().int32_v));
			DispatcherT::Trigger(pevt);
		}
		break;
		case SE_CONNECTED:
		{
			WAWO_ASSERT(evt->GetSocket() == m_socket);
			WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec));
			DispatcherT::Trigger(pevt1);

			WWRP<PeerEventT> pevt2(new PeerEventT(PE_CONNECTED, WWRP<MyT>(this), evt->GetSocket(), ec));
			DispatcherT::Trigger(pevt2);

			evt->GetSocket()->UnRegister(SE_ERROR, WWRP<ListenerT>(this));
		}
		break;
		case SE_CLOSE:
		{
			WAWO_ASSERT(evt->GetSocket() == m_socket);
			DetachSocket(evt->GetSocket());

			WWRP<PeerEventT> pevt1(new PeerEventT(PE_SOCKET_CLOSE, WWRP<MyT>(this), evt->GetSocket(), ec));
			DispatcherT::Trigger(pevt1);

			WWRP<PeerEventT> pevt2(new PeerEventT(PE_CLOSE, WWRP<MyT>(this), evt->GetSocket(), ec));
			DispatcherT::Trigger(pevt2);
		}
		break;
		case SE_ERROR:
		{
			WWRP<PeerEventT> pevt(new PeerEventT(PE_SOCKET_ERROR, WWRP<MyT>(this), evt->GetSocket(), ec));
			DispatcherT::Trigger(pevt);
		}
		break;
		default:
		{
			char tmp[256] = { 0 };
			snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown socket evt: %d", id);
			WAWO_THROW_EXCEPTION(tmp);
		}
		break;
		}
	}

	int Http::Get(WWSP<message::Http> const& message, WWRP<wawo::net::peer::HttpCallback_Abstract> const& cb, u32_t const& timeout ) {
		WAWO_ASSERT(message->GetType() == message::Http::T_REQUEST);
		WAWO_ASSERT(message->GetOption() == http::O_NONE);
		WAWO_ASSERT(cb != 0);

		message->SetOption(http::O_GET);

		//http pipeline not supported right now
		LockGuard<SpinMutex> lg(m_requested_mutex);

		if (!m_requested.empty()) {
			return wawo::E_HTTP_REQUEST_NOT_DONE;
		}
		int msndrt = MyT::DoSendMessage(message);
		WAWO_RETURN_V_IF_NOT_MATCH(msndrt, msndrt == wawo::OK);

		RequestedMessage reqm =
		{
			message,
			cb,
			wawo::time::curr_milliseconds(),
			timeout
		};
		m_requested.push(reqm);
		return msndrt;
	}

	int Http::Post(WWSP<message::Http> const& message, WWRP<wawo::net::peer::HttpCallback_Abstract> const& cb, u32_t const& timeout ) {
		WAWO_ASSERT(message->GetType() == message::Http::T_REQUEST);
		WAWO_ASSERT(message->GetOption() == http::O_NONE);
		WAWO_ASSERT(cb != 0);

		message->SetOption(http::O_POST);

		//http pipeline not supported right now
		LockGuard<SpinMutex> lg(m_requested_mutex);
		if (!m_requested.empty()) {
			return wawo::E_HTTP_REQUEST_NOT_DONE;
		}

		int msndrt = MyT::DoSendMessage(message);
		WAWO_RETURN_V_IF_NOT_MATCH(msndrt, msndrt == wawo::OK);

		RequestedMessage reqm =
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
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnMessageBegin();
	return 0;
}
inline static int on_message_complete(http_parser* parser) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnMessageComplete();
	return 0;
}

inline static int on_chunk_header(http_parser* parser) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnChunkHeader();
	return 0;
}

inline static int on_chunk_complete(http_parser* parser) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnChunkComplete();
	return 0;
}

inline static int on_status(http_parser* parser, char const* data, size_t length) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnStatus(data, length);
	return 0;
}
inline static int on_url(http_parser* parser, char const* data, size_t length) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnUrl(data, length);
	return 0;
}
inline static int on_body(http_parser* parser, char const* data, size_t length) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnBody(data, length);
	return 0;
}

inline static int on_header_field(http_parser* parser, char const* data, size_t length) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnHeaderField(data, length);
	return 0;
}

inline static int on_header_value(http_parser* parser, char const* data, size_t length) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnHeaderValue(data, length);
	return 0;
}

inline static int on_headers_complete(http_parser* parser) {
	wawo::net::peer::Http* http = (wawo::net::peer::Http*) parser->data;
	WAWO_ASSERT(http != NULL);
	http->OnHeaderComplete();
	return 0;
}
