#include <wawo/net/handler/websocket.hpp>

#ifdef ENABLE_WEBSOCKET

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>

#include <wawo/log/logger_manager.h>
#include <wawo/net/socket.hpp>

#define _H_Upgrade "Upgrade"
#define _H_Connection "Connection"
#define _H_SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version"
#define _H_SEC_WEBSOCKET_KEY "Sec-WebSocket-Key"
#define _H_SEC_WEBSOCKET_ACCEPT "Sec-WebSocket-Accept"


#define _WEBSOCKEET_UUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

namespace wawo { namespace net { namespace handler {

	static const char* WEBSOCKET_UPGRADE_REPLY_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";

	WWSP<packet> base64Encode(const char* src, u32_t const& len)
	{
		BIO *bmem = NULL;
		BIO *b64 = NULL;
		BUF_MEM *bptr;

		b64 = BIO_new(BIO_f_base64());
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

		bmem = BIO_new(BIO_s_mem());
		b64 = BIO_push(b64, bmem);
		BIO_write(b64, src, len);
		BIO_flush(b64);
		BIO_get_mem_ptr(b64, &bptr);
		BIO_set_close(b64, BIO_NOCLOSE);

		WWSP<packet> tmp = wawo::make_shared<packet>(bptr->length);
		tmp->write((byte_t*)bptr->data, bptr->length);

		BIO_free_all(b64);
		return tmp;
	}

	WWSP<packet> base64Decode(wawo::len_cstr const& to_dec )
	{
		BIO *b64 = NULL;
		BIO *bmem = NULL;

		char *buffer = (char *)::malloc(to_dec.len);
		memset(buffer, 0, to_dec.len);
		b64 = BIO_new(BIO_f_base64());
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

		bmem = BIO_new_mem_buf(to_dec.cstr, to_dec.len);
		bmem = BIO_push(b64, bmem);
		int rlen = BIO_read(bmem, buffer, to_dec.len);
		BIO_free_all(bmem);

		WWSP<packet> tmp = wawo::make_shared<packet>(to_dec.len);
		tmp->write( (byte_t*) buffer, rlen );

		::free(buffer);
		return tmp;
	}

	void SHA1(const char* src, u32_t const& len, unsigned char out[20] ) {
		SHA_CTX ctx;
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, src, len);
		SHA1_Final(out, &ctx);
	}

	void websocket::read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income)
	{
		int ec = 0;
_CHECK:
		switch (m_state) {
			
		case S_WAIT_CLIENT_HANDSHAKE_REQ:
			{
				WAWO_ASSERT( m_http_parser == NULL );
				m_http_parser = wawo::make_ref<wawo::net::protocol::http::parser>();
				m_http_parser->init(wawo::net::protocol::http::PARSER_REQ);

				m_http_parser->on_message_begin = std::bind(&websocket::http_on_message_begin, WWRP<websocket>(this));
				m_http_parser->on_url = std::bind(&websocket::http_on_url, WWRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_status = std::bind(&websocket::http_on_status, WWRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_header_field = std::bind(&websocket::http_on_header_field, WWRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_header_value = std::bind(&websocket::http_on_header_value, WWRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_headers_complete = std::bind(&websocket::http_on_headers_complete, WWRP<websocket>(this));

				m_http_parser->on_body = std::bind(&websocket::http_on_body, WWRP<websocket>(this), std::placeholders::_1, std::placeholders::_2);
				m_http_parser->on_message_complete = std::bind(&websocket::http_on_message_complete, WWRP<websocket>(this));

				m_http_parser->on_chunk_header = std::bind(&websocket::http_on_message_begin, WWRP<websocket>(this));
				m_http_parser->on_chunk_complete = std::bind(&websocket::http_on_message_begin, WWRP<websocket>(this));

				m_state = S_HANDSHAKING;
				goto _CHECK;
			}
		break;
		case S_HANDSHAKING:
			{
				u32_t nparsed = m_http_parser->parse( (char*) income->begin(), income->len(), ec );
				WAWO_ASSERT(ec == wawo::OK);
				WAWO_ASSERT(nparsed == income->len());
				goto _CHECK;
			}
			break;
		case S_UPGRADE_REQ_MESSAGE_DONE:
			{
				WAWO_ASSERT(m_upgrade_req != NULL);

				if (!m_upgrade_req->h.have(_H_Upgrade) || wawo::strcmp("websocket", m_upgrade_req->h.get(_H_Upgrade).cstr ) != 0 ) {
					WAWO_WARN("[websocket]missing %s, or not websocket, force close", _H_Upgrade);
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->so->close();
					return ;
				}

				if (!m_upgrade_req->h.have(_H_Connection) || wawo::strcmp("Upgrade", m_upgrade_req->h.get(_H_Connection).cstr) != 0) {
					WAWO_WARN("[websocket]missing %s, or not Upgrade, force close", _H_Connection);
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->so->close();
					return;
				}

				if (!m_upgrade_req->h.have(_H_SEC_WEBSOCKET_VERSION)) {
					WAWO_WARN("[websocket]missing Sec-WebSocket-Key, force close");
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*) WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400) );
					ctx->write(out);
					ctx->so->close();
					return;
				}

				if (!m_upgrade_req->h.have(_H_SEC_WEBSOCKET_KEY)) {
					WAWO_WARN("[websocket]missing Sec-WebSocket-Version, force close");
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->so->close();
					return;
				}

				wawo::len_cstr skey = m_upgrade_req->h.get(_H_SEC_WEBSOCKET_KEY) + _WEBSOCKEET_UUID;
				unsigned char sha1key[20];
				SHA1(skey.cstr, skey.len, sha1key);
				WWSP<packet> base64key = base64Encode((const char*)sha1key,20);

				WWSP<protocol::http::message> reply = wawo::make_shared<protocol::http::message>();
				reply->type = protocol::http::T_RESP;
				reply->status_code = 101;
				reply->status = "Switching Protocols";
				reply->ver = protocol::http::version{ 1,1 };
				
				reply->h.set(_H_Upgrade, "websocket");
				reply->h.set(_H_Connection, "Upgrade");
				reply->h.set(_H_SEC_WEBSOCKET_ACCEPT, wawo::len_cstr( (char*) base64key->begin(), base64key->len()) );
				
				WWSP<packet> o;
				protocol::http::encode_message(reply, o);
				ctx->write(o);

				m_state = S_OPEN;

				ctx->fire_connected();
				goto _CHECK;
			}
			break;
		case S_OPEN:
			{
				WAWO_INFO("<<< %u", income->len());
			}
			break;
		}
	}


	int websocket::http_on_message_begin() {
		WAWO_DEBUG("[%s]", __FUNCTION__);
		WAWO_ASSERT(m_upgrade_req == NULL);
		m_upgrade_req = wawo::make_shared<protocol::http::message>();
		return wawo::OK;
	}
	int websocket::http_on_url(const char* data, u32_t const& len) {
		WAWO_DEBUG("[%s]<<< %s",__FUNCTION__, wawo::len_cstr(data,len).cstr );

		m_upgrade_req->url = wawo::len_cstr(data, len);
		return wawo::OK;
	}

	int websocket::http_on_status(const char* data, u32_t const& len) {
		WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		WAWO_ASSERT(!"WHAT");
		return wawo::OK;
	}
	int websocket::http_on_header_field(const char* data, u32_t const& len) {
		WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		m_tmp_for_field = wawo::len_cstr(data, len);
		return wawo::OK;
	}
	int websocket::http_on_header_value(const char* data, u32_t const& len) {
		WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);

		m_upgrade_req->h.set(m_tmp_for_field, wawo::len_cstr(data, len));
		return wawo::OK;
	}
	int websocket::http_on_headers_complete() {
		WAWO_DEBUG(__FUNCTION__);
		return wawo::OK;
	}

	int websocket::http_on_body(const char* data, u32_t const& len) {
		WAWO_DEBUG("[%s]<<< %s", __FUNCTION__, wawo::len_cstr(data, len).cstr);
		return wawo::OK;
	}
	int websocket::http_on_message_complete() {
		WWSP<packet> m;
		protocol::http::encode_message(m_upgrade_req, m);
		WAWO_DEBUG("[%s], req: %s", __FUNCTION__, wawo::len_cstr( (char*) m->begin(), m->len() ) );

		m_state = S_UPGRADE_REQ_MESSAGE_DONE;
		return wawo::OK;
	}

	int websocket::http_on_chunk_header() {
		WAWO_DEBUG("[%s]", __FUNCTION__);
		return wawo::OK;
	}
	int websocket::http_on_chunk_complete() {
		WAWO_DEBUG("[%s]", __FUNCTION__);
		return wawo::OK;
	}

}}}

#endif //end of ENABLE_WEBSOCKET