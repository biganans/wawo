#ifndef _WAWO_NET_HANDLER_WEBSOCKET_HPP
#define _WAWO_NET_HANDLER_WEBSOCKET_HPP

#include <wawo/core.hpp>

#ifdef ENABLE_WEBSOCKET

#include <wawo/net/socket_handler.hpp>
#include <wawo/net/protocol/http.hpp>

namespace wawo { namespace net { namespace handler {

	class websocket :
		public wawo::net::socket_activity_handler_abstract,
		public wawo::net::socket_inbound_handler_abstract,
		public wawo::net::socket_outbound_handler_abstract
	{
		enum state {
			S_WAIT_CLIENT_HANDSHAKE_REQ,
			S_HANDSHAKING,
			S_UPGRADE_REQ_MESSAGE_DONE,
			S_MESSAGE_BEGIN,
			S_FRAME_BEGIN,
			S_FRAME_READ_H,
			S_FRAME_READ_PAYLOAD_LEN,
			S_FRAME_READ_MASKING_KEY,
			S_FRAME_READ_PAYLOAD,
			S_FRAME_END,
			S_MESSAGE_END
		};

		enum frame_opcode {
			OP_CONTINUE = 0x0,
			OP_TEXT = 0x1,
			OP_BINARY = 0x2,
			OP_CLOSE = 0x8,
			OP_PING = 0x9,
			OP_PONG = 0xA,
			OP_RESERVED_B = 0xB,
			OP_RESERVED_C = 0xC,
			OP_RESERVED_D = 0xD,
			OP_RESERVED_E = 0xE,
			OP_RESERVED_F = 0xF
		};

		struct ws_frame {
			struct _H {
				u8_t fin : 1;
				u8_t rsv1 : 1;
				u8_t rsv2 : 1;
				u8_t rsv3 : 1;
				u8_t opcode : 4;
				u8_t mask : 1;
				u8_t len : 7;
			} H;

			u64_t payload_len;
			u8_t masking_key_arr[4];

			WWSP<packet> extdata;
			WWSP<packet> appdata;
		};

		void decode_frame_H(WWSP<packet> const& p, ws_frame::_H &H) {
			WAWO_ASSERT(p->len() >= (sizeof(u8_t)*2) );
			u8_t B1 = p->read<u8_t>();

			H.fin = (B1 >> 7) & 0x1;
			H.rsv1 = (B1 >> 6) & 0x1;
			H.rsv2 = (B1 >> 5) & 0x1;
			H.rsv3 = (B1 >> 4) & 0x1;
			H.opcode = B1 & 0xF;

			u8_t B2 = p->read<u8_t>();

			H.mask = (B2 >> 7) & 0x1;
			H.len = B2 & 0x7F;
		}

		void encode_frame_H(ws_frame::_H const& H, WWSP<packet>& o) {
			u8_t B1 = 0;

			B1 |= (H.fin & 0x1) << 7;
			B1 |= (H.rsv1 & 0x1) << 6;
			B1 |= (H.rsv2 & 0x1) << 5;
			B1 |= (H.rsv3 & 0x1) << 4;
			B1 |= (H.opcode & 0xF);
			o->write(B1);

			u8_t B2 = 0;
			B2 |= (H.mask & 0x1) << 7;
			B2 |= (H.len & 0x7F);
			o->write(B2);
		}

		wawo::len_cstr m_tmp_for_field;
		WWSP<protocol::http::message> m_upgrade_req;
		state m_state;
		WWRP<wawo::net::protocol::http::parser> m_http_parser;
		WWSP<wawo::packet> m_income_prev;
		WWSP<ws_frame> m_tmp_frame;


		WWSP<packet> m_tmp_message; //for fragmented message
		u8_t m_fragmented_opcode;
		u8_t m_fragmented_begin;
		
	public:
			websocket() :
				m_state(S_WAIT_CLIENT_HANDSHAKE_REQ),
				m_http_parser(NULL)
			{}

			virtual ~websocket() {}
			void connected(WWRP<socket_handler_context> const& ctx);

			void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) ;
			void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet);

		protected:
			int http_on_message_begin();
			int http_on_url(const char* data, u32_t const& len);
			int http_on_status(const char* data, u32_t const& len);
			int http_on_header_field(const char* data, u32_t const& len);
			int http_on_header_value(const char* data, u32_t const& len);
			int http_on_headers_complete();
			int http_on_body(const char* data, u32_t const& len);
			int http_on_message_complete();

			int http_on_chunk_header();
			int http_on_chunk_complete();
		};
}}}

#endif //endfor ENABLE_WEBSOCKET
#endif