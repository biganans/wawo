#ifndef _WAWO_NET_PEER_PROTOCOL_SYMMETRIC_ENCRYPT_HPP
#define _WAWO_NET_PEER_PROTOCOL_SYMMETRIC_ENCRYPT_HPP

#include <wawo/algorithm/Packet.hpp>
#include <wawo/security/Cipher_Abstract.hpp>
#include <wawo/net/core/TLP/HLenPacket.hpp>

namespace wawo { namespace net { namespace core { namespace TLP {

	using namespace wawo::algorithm;
	using namespace wawo::net::core;

	class SymmetricEncrypt :
		public TLP::HLenPacket
	{
		WWSP<wawo::security::Cipher_Abstract> m_cipher;

	public:
		SymmetricEncrypt() {}
		virtual ~SymmetricEncrypt() {}

		void SetCipher(WWSP<wawo::security::Cipher_Abstract> const& cipher) {
			m_cipher = cipher;
		}

		inline WWSP<wawo::security::Cipher_Abstract> const& GetCipher() const { return m_cipher; }

		int Encode(WWSP<Packet> const& in, WWSP<Packet>& out ) {
			WAWO_ASSERT( m_cipher != NULL );
			WWSP<Packet> encrypted;
			int rt =  m_cipher->Encrypt(in, encrypted);
			WAWO_RETURN_V_IF_NOT_MATCH(rt, rt==wawo::OK );
			return HLenPacket::Encode(encrypted, out);
		}

		u32_t DecodePackets( wawo::algorithm::BytesRingBuffer* const buffer, WWSP<Packet> outs[], u32_t const& size, int& ec_o) {
			WAWO_ASSERT(m_cipher != NULL);

			u32_t pcount = 0;
			WWSP<Packet> out;
			u32_t decodert = HLenPacket::DecodePackets(buffer, outs, size, ec_o,out);
			for (u32_t i = 0; i < decodert; i++) {
				WWSP<Packet> decrypted;
				int decryptrt = m_cipher->Decrypt( outs[i], decrypted);
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

}}}}
#endif