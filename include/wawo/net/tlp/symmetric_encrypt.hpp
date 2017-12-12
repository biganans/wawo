#ifndef _WAWO_NET_PEER_PROTOCOL_SYMMETRIC_ENCRYPT_HPP
#define _WAWO_NET_PEER_PROTOCOL_SYMMETRIC_ENCRYPT_HPP

#include <wawo/packet.hpp>
#include <wawo/security/cipher_abstract.hpp>
#include <wawo/net/tlp/hlen_packet.hpp>

namespace wawo { namespace net { namespace tlp {

	class symmetric_encrypt :
		public tlp::hlen_packet
	{
		WAWO_DECLARE_NONCOPYABLE(symmetric_encrypt)
		WWSP<wawo::security::cipher_abstract> m_cipher;

	public:
		symmetric_encrypt() {}
		virtual ~symmetric_encrypt() {}

		void set_cipher(WWSP<wawo::security::cipher_abstract> const& cipher) {
			m_cipher = cipher;
		}

		inline WWSP<wawo::security::cipher_abstract> const& get_cipher() const { return m_cipher; }

		int encode(WWSP<packet> const& in, WWSP<packet>& out ) {
			WAWO_ASSERT( m_cipher != NULL );
			WWSP<packet> encrypted;
			int rt =  m_cipher->encrypt(in, encrypted);
			WAWO_RETURN_V_IF_NOT_MATCH(rt, rt==wawo::OK );
			return hlen_packet::encode(encrypted, out);
		}

		u32_t decode_packets(WWRP<bytes_ringbuffer> const& buffer, WWSP<packet> outs[], u32_t const& size, int& ec_o) {
			WAWO_ASSERT(m_cipher != NULL);

			u32_t pcount = 0;
			WWSP<packet> out;
			u32_t decodert = hlen_packet::decode_packets(buffer, outs, size, ec_o,out);
			for (u32_t i = 0; i < decodert; ++i) {
				WWSP<packet> decrypted;
				int decryptrt = m_cipher->decrypt( outs[i], decrypted);
				if (decryptrt != wawo::OK) {
					ec_o = decryptrt;
					break;
				}
				WAWO_ASSERT(pcount<size);
				outs[pcount++] = decrypted;
			}
			return pcount;
		}
	};

}}}
#endif