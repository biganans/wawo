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

	void websocket::read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income)
	{
		if (WAWO_UNLIKELY(!m_inited)) {
			m_income_prev = wawo::make_shared<packet>();
			m_tmp_frame = wawo::make_shared<ws_frame>();
			m_tmp_frame->appdata = wawo::make_shared<packet>();
			m_tmp_frame->extdata = wawo::make_shared<packet>();

			m_tmp_message = wawo::make_shared<packet>();
			ctx->ch->turnon_nodelay();

			m_inited = true;
		}

		if (income->len() == 0) {
			WAWO_ASSERT(!"WHAT");
			return;
		}

		if ( m_income_prev->len() ) {
			income->write_left(m_income_prev->begin(), m_income_prev->len());
			m_income_prev->reset();
		}

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
				income->skip(nparsed);
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
					ctx->close();
					return ;
				}

				if (!m_upgrade_req->h.have(_H_Connection) || wawo::strcmp("Upgrade", m_upgrade_req->h.get(_H_Connection).cstr) != 0) {
					WAWO_WARN("[websocket]missing %s, or not Upgrade, force close", _H_Connection);
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->close();
					return;
				}

				if (!m_upgrade_req->h.have(_H_SEC_WEBSOCKET_VERSION)) {
					WAWO_WARN("[websocket]missing Sec-WebSocket-Key, force close");
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*) WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400) );
					ctx->write(out);
					ctx->close();
					return;
				}

				if (!m_upgrade_req->h.have(_H_SEC_WEBSOCKET_KEY)) {
					WAWO_WARN("[websocket]missing Sec-WebSocket-Version, force close");
					WWSP<packet> out = wawo::make_shared<packet>();
					out->write((byte_t*)WEBSOCKET_UPGRADE_REPLY_400, wawo::strlen(WEBSOCKET_UPGRADE_REPLY_400));
					ctx->write(out);
					ctx->close();
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

				m_state = S_MESSAGE_BEGIN;
				ctx->fire_connected();

				goto _CHECK;
			}
			break;
		case S_MESSAGE_BEGIN:
			{
				WAWO_ASSERT(m_tmp_message != NULL);
				
				m_state = S_FRAME_BEGIN;
				m_fragmented_opcode = OP_TEXT; //default
				m_fragmented_begin = false;
				goto _CHECK;
			}
			break;
		case S_FRAME_BEGIN:
			{
				m_state = S_FRAME_READ_H;
				goto _CHECK;
			}
		case S_FRAME_READ_H:
			{
				//WAWO_INFO("<<< %u", income->len());
				WAWO_ASSERT(sizeof(ws_frame::H) == 2, "%u", sizeof(ws_frame::H));
				if (income->len() >= sizeof(ws_frame::H)) {

					m_tmp_frame->H.B1.B = income->read<u8_t>();
					m_tmp_frame->H.B2.B = income->read<u8_t>();

					//@Note: refer to RFC6455
					//frame with a control opcode would not be fragmented
					if (m_tmp_frame->H.B1.Bit.fin == 0 && m_fragmented_begin == false ) {
						WAWO_ASSERT( m_tmp_frame->H.B1.Bit.opcode != 0x0);
						m_fragmented_opcode = m_tmp_frame->H.B1.Bit.opcode;
						m_fragmented_begin = true;
					}

					m_state = S_FRAME_READ_PAYLOAD_LEN;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case S_FRAME_READ_PAYLOAD_LEN:
			{
				//WAWO_INFO("<<< %u", income->len());
				if (m_tmp_frame->H.B2.Bit.len == 126) {
					if (income->len() >= sizeof(u16_t)) {
						m_tmp_frame->payload_len = income->read<u16_t>();
						m_state = S_FRAME_READ_MASKING_KEY;
						goto _CHECK;
					}
				}
				else if (m_tmp_frame->H.B2.Bit.len == 127) {
					if (income->len() >= sizeof(u64_t)) {
						m_tmp_frame->payload_len = income->read<u64_t>();
						m_state = S_FRAME_READ_MASKING_KEY;
						goto _CHECK;
					}
				}
				else {
					m_tmp_frame->payload_len = m_tmp_frame->H.B2.Bit.len;
					m_state = S_FRAME_READ_MASKING_KEY;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case S_FRAME_READ_MASKING_KEY:
			{
				if (income->len() >= sizeof(u32_t)) {
					u32_t key = income->read<u32_t>();

					m_tmp_frame->masking_key_arr[0] = (key >> 24) & 0xff;
					m_tmp_frame->masking_key_arr[1] = (key >> 16) & 0xff;
					m_tmp_frame->masking_key_arr[2] = (key >> 8) & 0xff;
					m_tmp_frame->masking_key_arr[3] = (key) & 0xff;

					m_state = S_FRAME_READ_PAYLOAD;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case S_FRAME_READ_PAYLOAD:
			{
				u64_t left_to_fill = m_tmp_frame->payload_len - m_tmp_frame->appdata->len();
				if (income->len() >= left_to_fill) {

					if (m_tmp_frame->H.B2.Bit.mask == 0x1) {
						u64_t i = 0;
						while (i < left_to_fill) {
							u8_t _byte = *(income->begin() + (i++)) ^ m_tmp_frame->masking_key_arr[i % 4];
							m_tmp_frame->appdata->write<u8_t>(_byte);
						}
					} else {
						m_tmp_frame->appdata->write(income->begin(), left_to_fill);
					}

					income->skip(left_to_fill);
					m_state = S_FRAME_END;
					goto _CHECK;
				}
				m_income_prev = income;
			}
			break;
		case S_FRAME_END:
			{
				//NO EXT SUPPORT NOW
				WAWO_ASSERT(m_tmp_frame->H.B1.Bit.rsv1 == 0);
				WAWO_ASSERT(m_tmp_frame->H.B1.Bit.rsv2 == 0);
				WAWO_ASSERT(m_tmp_frame->H.B1.Bit.rsv3 == 0);

				switch (m_tmp_frame->H.B1.Bit.opcode) {
				case OP_CLOSE:
					{
						//reply a CLOSE, then do ctx->close();
						WAWO_DEBUG("<<< op_close");
						close(ctx);
						m_state = S_FRAME_CLOSE_RECEIVED;
					}
					break;
				case OP_PING:
					{
						//reply a PONG
						WWSP<ws_frame> _PONG = wawo::make_shared<ws_frame>();
						_PONG->H.B1.Bit.fin = 0x1;
						_PONG->H.B1.Bit.rsv1 = 0x0;
						_PONG->H.B1.Bit.rsv2 = 0x0;
						_PONG->H.B1.Bit.rsv3 = 0x0;
						_PONG->H.B1.Bit.opcode = OP_PONG;

						_PONG->H.B2.Bit.mask = 0x0;
						_PONG->H.B2.Bit.len = m_tmp_frame->appdata->len();
						_PONG->appdata = m_tmp_frame->appdata;

						WWSP<packet> outp_PONG = wawo::make_shared<packet>();
						outp_PONG->write<u8_t>( _PONG->H.B1.B );
						outp_PONG->write<u8_t>( _PONG->H.B2.B);

						outp_PONG->write(m_tmp_frame->appdata->begin(), m_tmp_frame->appdata->len());
						m_tmp_frame->appdata->reset();

						ctx->write(outp_PONG);
						m_state = S_FRAME_BEGIN;
						goto _CHECK;
					}
					break;
				case OP_PONG:
					{
						//ignore right now
						m_state = S_FRAME_BEGIN;
						goto _CHECK;
					}
					break;
				case OP_TEXT:
				case OP_BINARY:
				case OP_CONTINUE:
					{
						WAWO_ASSERT(m_tmp_frame->payload_len == m_tmp_frame->appdata->len());
						m_tmp_message->write(m_tmp_frame->appdata->begin(), m_tmp_frame->appdata->len());
						m_tmp_frame->appdata->reset();

						if (m_tmp_frame->H.B1.Bit.fin == 0x1) {
							m_state = S_MESSAGE_END;
						}
						else {
							m_state = S_FRAME_BEGIN;
						}
						goto _CHECK;
					}
					break;
				default:
					{
						WAWO_THROW("unknown websocket opcode");
					}
				}
			}
			break;
		case S_MESSAGE_END:
			{
				ctx->fire_read(m_tmp_message);
				m_tmp_message->reset();

				m_state = S_MESSAGE_BEGIN;
				goto _CHECK;
			}
			break;
		case S_FRAME_CLOSE_RECEIVED:
			{/*exit point*/}
			break;
		}
	}


	int websocket::write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet) {

		WWSP<ws_frame> _frame = wawo::make_shared<ws_frame>();
		_frame->H.B1.Bit.fin = 0x1;
		_frame->H.B1.Bit.rsv1 = 0x0;
		_frame->H.B1.Bit.rsv2 = 0x0;
		_frame->H.B1.Bit.rsv3 = 0x0;
		_frame->H.B1.Bit.opcode = OP_TEXT;
		_frame->H.B2.Bit.mask = 0x0;

		if (outlet->len() > 0xFFFF) {
			_frame->H.B2.Bit.len = 0x7F;
			outlet->write_left<u64_t>(outlet->len());
			outlet->write_left<u8_t>( _frame->H.B2.B );
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}
		else if (outlet->len() > 0x7D) {
			WAWO_ASSERT(outlet->len() <= 0xFFFF);
			_frame->H.B2.Bit.len = 0x7E;
			outlet->write_left<u16_t>(outlet->len());
			outlet->write_left<u8_t>(_frame->H.B2.B);
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}
		else {
			_frame->H.B2.Bit.len = outlet->len();
			outlet->write_left<u8_t>(_frame->H.B2.B);
			outlet->write_left<u8_t>(_frame->H.B1.B);
		}

		return ctx->write(outlet);
	}

	int websocket::close(WWRP<channel_handler_context> const& ctx, int const& code ) {

		if (m_close_sent == true) { return wawo::E_HANDLER_WEBSOCKET_CLOSED_ALREADY; }

		WWSP<ws_frame> _CLOSE = wawo::make_shared<ws_frame>();
		_CLOSE->H.B1.Bit.fin = 0x1;
		_CLOSE->H.B1.Bit.rsv1 = 0x0;
		_CLOSE->H.B1.Bit.rsv2 = 0x0;
		_CLOSE->H.B1.Bit.rsv3 = 0x0;
		_CLOSE->H.B1.Bit.opcode = OP_CLOSE;

		_CLOSE->H.B2.Bit.mask = 0x0;
		_CLOSE->H.B2.Bit.len = 0x0;

		WWSP<packet> outp_CLOSE = wawo::make_shared<packet>();
		outp_CLOSE->write<u8_t>( _CLOSE->H.B1.B );
		outp_CLOSE->write<u8_t>( _CLOSE->H.B2.B );

		ctx->write(outp_CLOSE);
		m_close_sent = true;
		return ctx->close(code);
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