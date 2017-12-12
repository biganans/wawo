#ifndef _WAWO_NET_TLP_DH_SYMMETRIC_ENCRYPT_HPP
#define _WAWO_NET_TLP_DH_SYMMETRIC_ENCRYPT_HPP

#include <wawo/net/tlp/symmetric_encrypt.hpp>
#include <wawo/security/dh.hpp>
#include <wawo/security/obfuscate.hpp>
#include <wawo/security/xxtea.hpp>


#ifdef WAWO_ENABLE_TRACE_DH
	#define WAWO_TRACE_DH WAWO_DEBUG
#else
	#define WAWO_TRACE_DH(...)
#endif

namespace wawo { namespace  net { namespace tlp {

	/*
	 * KEY_EXCHANGE frame
	 * 
	 * any data except frame 1 arrived before KEY_GENERATED state would be taken as invalid

	enum symmetric_cipher_suite {
	  CIPHER_XXTEA,
	  CIPHER_AES_256
	};

	frame_handshake_hello
	{
		u64_t:random_id,
		symmetric_cipher_suite_list<CIPHER_ID1,CIPHER_ID2,...>,
		u8_t:compress_method,
	}
	frame_handshake_hello_resp
	{
		u64_t:random_id,
		uint8:symmetric_cipher_suite_id,
		u8_t:compress_method,
	}
	frame_handshake_ok
	{
		encrypted( "MESSAGE" ),
	}
	if the remote side decrypted success , it would get "MESSAGE", otherwise, remote side should close connection

	frame_handshake_error
	{
		i32_t:code
	}
	frame_data
	{
	}

	//below frame not implementeds right now
	frame_change_key_request
	{
	u64_t:random_id
	}
	frame_change_key_response
	{
	u64_t:random_id
	}

	 * KEY EXCHANGE & symmetric cipher choice machnism
	 * A connect to B
	 * A send hello_server message to B
	 * B send hello_client message to A
	 * B generate its own private key, setup compress method, send handshake_ok message to A
	 * A generate its own private key, setup compress method, send handshake_ok message to B
	 *
	 * from now on, A <-> B can transfer data between them
	 */

#define WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
	enum DH_FrameType {
		F_HANDSHAKE_HELLO = 1,
		F_HANDSHAKE_HELLO_RESP = 2,
		F_HANDSHAKE_OK = 3,
		F_HANDSHAKE_ERROR = 5,

		//below frame not implemented yet
		F_CHANGE_KEY = 7,
		F_CHANGE_KEY_OK = 9,
		F_DATA = 11
	};

	enum DH_Cipher_Suite {
		CS_NONE = 1,
		CS_XXTEA = 2, //the only supported one right now
		
		CS_MAX = 3
	};
	enum DH_Compress {
		C_NONE = 1,
		C_GZIP = 2,
		C_DFLATE = 3,

		C_MAX = 4
	};
#else
	enum DH_FrameType {
		F_HANDSHAKE_HELLO = 0x01 << 0,
		F_HANDSHAKE_HELLO_RESP = 0x01 << 1,
		F_HANDSHAKE_OK = 0x01 << 2,
		F_HANDSHAKE_ERROR = 0x01 << 3,

		//below frame not implemented yet
		F_CHANGE_KEY = 0x01 << 4,
		F_CHANGE_KEY_OK = 0x01 << 5,
		F_DATA = 0x01 << 6
	};

	enum DH_Cipher_Suite {
		CS_NONE,
		CS_XXTEA, //the only supported one right now
		CS_MAX
	};
	enum DH_Compress {
		C_NONE,
		C_GZIP,
		C_DFLATE,
		C_MAX
	};
#endif

	inline WWSP<wawo::security::cipher_abstract> make_cipher_suite(DH_Cipher_Suite const& id) {
		switch (id)
		{
			case CS_XXTEA:
			{
				return wawo::make_shared<wawo::security::xxtea>();
			}
			break;
			default:
			{
				return NULL;
			}
			break;
		}
	}

	struct DH_Context {
		wawo::security::DH_Factor dhfactor;
		u8_t cipher_count;
		DH_Cipher_Suite ciphers[CS_MAX];
		u8_t compress_count;
		DH_Compress compress[C_MAX];
	};

	struct DH_Frame {
		u8_t frame;
		WWSP<packet> fpacket;
		DH_Frame(u8_t const& f) :
			frame(f) {}

		~DH_Frame() {}
	};
	struct DH_FrameHandshakeHello:
		public DH_Frame
	{
		DH_FrameHandshakeHello() :DH_Frame(F_HANDSHAKE_HELLO) {}

		u64_t pub_key;
		u8_t cipher_count;
		DH_Cipher_Suite ciphers[CS_MAX];
		u8_t compress_count;
		DH_Compress compress[C_MAX];
	};

	struct DH_FrameHandshakeHelloResp:
		public DH_Frame
	{
		DH_FrameHandshakeHelloResp() :DH_Frame(F_HANDSHAKE_HELLO_RESP) {}

		u64_t pub_key;
		DH_Cipher_Suite cipher;
		DH_Compress compress;
	};

	const static char* HOK_MESSAGE = "MESSAGE";
	struct DH_FrameHandshakeOk:
		public DH_Frame
	{
		DH_FrameHandshakeOk() :DH_Frame(F_HANDSHAKE_OK) {}
	};

	struct DH_FrameData:
		public DH_Frame
	{
		DH_FrameData() :DH_Frame(F_DATA) {}
	};

	class dh_symmetric_encrypt :
		public symmetric_encrypt
	{

	public:
		enum DHState {
			DH_IDLE,
			DH_HANDSHAKE,
			DH_DATA_TRANSFER,
			DH_CHANGE_KEY,
			DH_ERROR
		};

		DHState m_dhstate;
		DH_Context m_context;

		void* m_cookie;
		u32_t m_cookie_size;

		dh_symmetric_encrypt():m_dhstate(DH_IDLE),m_cookie(NULL),m_cookie_size(0) {
			::memset(&m_context, 0, sizeof(m_context));
		}
		virtual ~dh_symmetric_encrypt() {
			::free(m_cookie);
			m_cookie = NULL;
		}

		void handshake_assign_cookie(void const* const cookie, u32_t const& size) {
			WAWO_ASSERT(cookie != NULL);
			m_cookie = (void*)::malloc(m_cookie_size);
			WAWO_ALLOC_CHECK(m_cookie, m_cookie_size);
			::memcpy(m_cookie,cookie,size);
			m_cookie_size = size;
		}

		virtual int handshake_make_hello_packet( WWSP<packet>& hello) {

			WAWO_ASSERT(hello == NULL);

			if (m_cookie != NULL) {
				WAWO_ASSERT(sizeof(DH_Context) == m_cookie_size);
				::memcpy(&m_context, m_cookie, m_cookie_size);
			}else{
				m_context.cipher_count = 2;
				m_context.ciphers[0] = CS_XXTEA;
				m_context.ciphers[1] = CS_NONE;
				m_context.compress_count = 1;
				m_context.compress[0] = C_NONE;
			}
			while (m_context.dhfactor.pub_key == 0 || m_context.dhfactor.priv_key == 0) {
				m_context.dhfactor = wawo::security::dh_generate_factor();
			}
			WAWO_TRACE_DH("[dh_symmetric_encrypt]pub key: %llu,priv key: %llu", m_context.dhfactor.pub_key, m_context.dhfactor.priv_key );

			DH_FrameHandshakeHello fhello;
			fhello.pub_key = m_context.dhfactor.pub_key;

			fhello.cipher_count = m_context.cipher_count;
			for (u8_t i = 0; i < m_context.cipher_count; ++i) {
				fhello.ciphers[i] = m_context.ciphers[i];
			}
			fhello.compress_count = m_context.compress_count;
			for (u8_t i = 0; i < m_context.compress_count; ++i) {
				fhello.compress[i] = m_context.compress[i];
			}

			WWSP<packet> hello_packet = wawo::make_shared<packet>();
			
#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
			hello_packet->write<u32_t>( security::u8_obfuscate(fhello.frame,wawo::random_u32()) );
#else
			hello_packet->write<u8_t>(fhello.frame);
#endif

			hello_packet->write<u64_t>(fhello.pub_key);

			hello_packet->write<u8_t>(fhello.cipher_count);
			for (u8_t i = 0; i < fhello.cipher_count; ++i) {
				hello_packet->write<u8_t>(fhello.ciphers[i]&0xff);
			}
			hello_packet->write<u8_t>(fhello.compress_count);
			for (u8_t i = 0; i < fhello.compress_count; ++i) {
				hello_packet->write<u8_t>(fhello.compress[i]&0xff);
			}

			WWSP<packet> hlength_hello;
			hlen_packet::encode(hello_packet, hlength_hello);
			m_dhstate = DH_HANDSHAKE;

			hello = hlength_hello;
			return wawo::E_TLP_HANDSHAKING;
		}

		virtual int handshake_packet_arrive(WWSP<packet> const& in, WWSP<packet>& out) {
			WAWO_ASSERT(m_dhstate != DH_DATA_TRANSFER);

#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
			WAWO_RETURN_V_IF_NOT_MATCH( wawo::E_TLP_INVALID_PACKET, in->len() > sizeof(u32_t) );
			u8_t dh_frame = security::u8_deobfuscate(in->read<u32_t>());
#else
			WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, in->len() > sizeof(u8_t));
			u8_t dh_frame = in->read<u8_t>();
#endif
			switch (dh_frame) {
			case F_HANDSHAKE_HELLO:
			{
				//generate key , generate resp, generate MESSAGE
				WAWO_ASSERT( m_dhstate == DH_IDLE );
				m_dhstate = DH_HANDSHAKE;

				DH_FrameHandshakeHello hello;
				hello.pub_key = in->read<u64_t>();
				WAWO_TRACE_DH("[dh_symmetric_encrypt]received pub key: %llu", hello.pub_key );

				hello.cipher_count = in->read<u8_t>();
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, in->len()>hello.cipher_count);
				for (u8_t i = 0; i < hello.cipher_count; ++i) {
					hello.ciphers[i] = (DH_Cipher_Suite)in->read<u8_t>();
				}

				hello.compress_count = in->read<u8_t>();
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, in->len()>=hello.compress_count);
				for (u8_t i = 0; i < hello.compress_count; ++i) {
					hello.compress[i] = (DH_Compress)in->read<u8_t>();
				}
				WAWO_ASSERT(in->len() == 0 );
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, in->len()==0);

				while (m_context.dhfactor.pub_key == 0 || m_context.dhfactor.priv_key == 0) {
					m_context.dhfactor = wawo::security::dh_generate_factor();
				}
				WAWO_TRACE_DH("[dh_symmetric_encrypt]pub key: %llu,priv key: %llu", m_context.dhfactor.pub_key, m_context.dhfactor.priv_key);

				DH_FrameHandshakeHelloResp hello_resp;
				hello_resp.pub_key = m_context.dhfactor.pub_key;
				hello_resp.cipher = hello.ciphers[0];
				hello_resp.compress = hello.compress[0];

				WWSP<wawo::security::cipher_abstract> cipher = make_cipher_suite(hello.ciphers[0]);
				WAWO_RETURN_V_IF_NOT_MATCH( wawo::E_TLP_UNKNOWN_CIPHER, cipher != NULL );
				u64_t key = wawo::security::dh_generate_key(hello.pub_key, m_context.dhfactor.priv_key);
				WAWO_TRACE_DH("[dh_symmetric_encrypt]generate key: %llu", key);
				byte_t bkey[16] = { 0 };
				wawo::bytes_helper::write_u64(key, (byte_t*)&bkey[0]);
				cipher->set_key(bkey, sizeof(u64_t));

				set_cipher(cipher);

				WWSP<packet> OOO ;
				WWSP<packet> hello_resp_packet = wawo::make_shared<packet>();

#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
				hello_resp_packet->write<u32_t>(security::u8_obfuscate(hello_resp.frame, wawo::random_u32()));
#else
				hello_resp_packet->write<u8_t>(hello_resp.frame);
#endif
				hello_resp_packet->write<u64_t>(hello_resp.pub_key);
				hello_resp_packet->write<u8_t>(hello_resp.cipher&0xff);
				hello_resp_packet->write<u8_t>(hello_resp.compress&0xff);
				hlen_packet::encode(hello_resp_packet,OOO);

				WWSP<packet> message = wawo::make_shared<packet>();
				message->write( (byte_t*)HOK_MESSAGE, wawo::strlen(HOK_MESSAGE) );
				WWSP<packet> encrypted_message;
				WWSP<packet> hlength_encrypted_message;

				int encrt = cipher->encrypt(message,encrypted_message);
				WAWO_RETURN_V_IF_NOT_MATCH(encrt, encrt == wawo::OK );
#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
				encrypted_message->write_left<u32_t>(security::u8_obfuscate(F_HANDSHAKE_OK,wawo::random_u32()));
#else
				encrypted_message->write_left<u8_t>(F_HANDSHAKE_OK);
#endif
				hlen_packet::encode(encrypted_message, hlength_encrypted_message);

				OOO->write( hlength_encrypted_message->begin(), hlength_encrypted_message->len() );
				out = OOO;

				return wawo::E_TLP_HANDSHAKING;
			}
			break;
			case F_HANDSHAKE_HELLO_RESP:
			{
				WAWO_ASSERT(m_dhstate == DH_HANDSHAKE);
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, (in->len() >= sizeof(u64_t)+sizeof(u8_t)*2) );

				//generate key, send MESSAGE
				DH_FrameHandshakeHelloResp hello_resp;
				hello_resp.pub_key = in->read<u64_t>();
				WAWO_TRACE_DH("[dh_symmetric_encrypt]received pub key: %llu", hello_resp.pub_key);

				hello_resp.cipher = (DH_Cipher_Suite)in->read<u8_t>();
				hello_resp.compress = (DH_Compress)in->read<u8_t>();
				WAWO_ASSERT(in->len() == 0);
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_INVALID_PACKET, in->len() == 0);

				WWSP<wawo::security::cipher_abstract> cipher = make_cipher_suite(hello_resp.cipher);
				WAWO_RETURN_V_IF_NOT_MATCH(wawo::E_TLP_UNKNOWN_CIPHER, cipher != NULL);
				WAWO_ASSERT(m_context.dhfactor.priv_key!=0);
				u64_t key = wawo::security::dh_generate_key(hello_resp.pub_key, m_context.dhfactor.priv_key);
				WAWO_TRACE_DH("[dh_symmetric_encrypt]generate key: %llu", key);

				byte_t bkey[16] = { 0 };
				wawo::bytes_helper::write_u64(key, (byte_t*)&bkey[0]);
				cipher->set_key(bkey, sizeof(u64_t) );

				set_cipher(cipher);

				WWSP<packet> message = wawo::make_shared<packet>();
				message->write((byte_t*)HOK_MESSAGE, wawo::strlen(HOK_MESSAGE));
				WWSP<packet> encrypted_message;
				WWSP<packet> hlength_encrypted_message;

				int encrt = cipher->encrypt(message, encrypted_message);

				//WWSP<packet> mm;
				//int decrt = cipher->decrypt(encrypted_message,mm);

				WAWO_RETURN_V_IF_NOT_MATCH(encrt, encrt == wawo::OK);
#ifdef WAWO_DH_SYMMETRIC_ENCRYPT_ENABLE_HANDSHAKE_OBFUSCATE
				encrypted_message->write_left<u32_t>(security::u8_obfuscate(F_HANDSHAKE_OK, wawo::random_u32()));
#else
				encrypted_message->write_left<u8_t>(fhello.frame);
#endif
				hlen_packet::encode(encrypted_message, hlength_encrypted_message);
				out = hlength_encrypted_message;

				return wawo::E_TLP_HANDSHAKING;
			}
			break;
			case F_HANDSHAKE_OK:
			{
				WAWO_ASSERT(m_dhstate == DH_HANDSHAKE);
				//check decrypted(message) == "MESSAGE"

				WWSP<packet> message;
				WAWO_ASSERT(get_cipher() != NULL);
				int decryptrt = get_cipher()->decrypt(in, message);
				WAWO_RETURN_V_IF_NOT_MATCH(decryptrt, decryptrt==wawo::OK);

				if (wawo::strncmp((char*)message->begin(), HOK_MESSAGE, wawo::strlen(HOK_MESSAGE)) == 0) {
					//handshake done
					m_dhstate = DH_DATA_TRANSFER;
					return wawo::E_TLP_HANDSHAKE_DONE;
				}

				return wawo::E_TLP_HANDSHAKE_MESSAGE_CHECK_FAILED;
			}
			break;
			default:
			{
				return wawo::E_TLP_INVALID_PACKET;
			}
			break;
			}
		}

		bool handshake_handshaking() const { return m_dhstate == DH_HANDSHAKE; }
		bool handshake_done() const { return m_dhstate == DH_DATA_TRANSFER; }

		int encode(WWSP<packet> const& in, WWSP<packet>& out) {
			if (m_dhstate != DH_DATA_TRANSFER) {
				return wawo::E_TLP_INVALID_STATE ;
			}

			return symmetric_encrypt::encode(in, out);
		}

		//only cast non-handleshake packet out
		u32_t decode_packets(WWRP<bytes_ringbuffer> const& buffer, WWSP<packet> arrives[], u32_t const& size, int& ec_o, WWSP<packet>& out) {

			WAWO_ASSERT(buffer != NULL );
			WAWO_ASSERT(arrives != NULL);
			u32_t pcount = 0;
			switch (m_dhstate) {
				case DH_DATA_TRANSFER:
				{
					WAWO_ASSERT(size > 0);
					pcount = symmetric_encrypt::decode_packets(buffer, arrives, size, ec_o);
				}
				break;
				case DH_IDLE:
				case DH_HANDSHAKE:
				{
					WWSP<packet> handshakeout ;
					u32_t count = hlen_packet::decode_packets(buffer, arrives, 1, ec_o, handshakeout);
					if (count == 1) {
						ec_o = handshake_packet_arrive(arrives[0], handshakeout);
						out = handshakeout;
					}
				}
				break;
				case DH_CHANGE_KEY:
				{//@to impl
				}
				break;
				case DH_ERROR:
				{//@to impl
				}
				break;
			}
			return pcount;
		}
	};
}}}
#endif