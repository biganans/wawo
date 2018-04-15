#ifndef _WAWO_NET_HANDLER_SYMMETRIC_ENCRYPT_HPP
#define _WAWO_NET_HANDLER_SYMMETRIC_ENCRYPT_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>

#include <wawo/security/cipher_abstract.hpp>

namespace wawo { namespace net { namespace handler {

	//packet based encrypt/decrypt
	class symmetric_encrypt :
		public wawo::net::channel_inbound_handler_abstract,
		public wawo::net::channel_outbound_handler_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(symmetric_encrypt)
		WWSP<wawo::security::cipher_abstract> m_cipher ;

	public:
		symmetric_encrypt() {}
		virtual ~symmetric_encrypt() {}

		void assign_cipher(WWSP<wawo::security::cipher_abstract> const& cipher) {
			m_cipher = cipher;
		}

		WWSP<wawo::security::cipher_abstract> cipher() const {
			return m_cipher;
		}

		void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income ) {
			WAWO_ASSERT(m_cipher != NULL);
			WWRP<packet> decrypted;
			int decryptrt = m_cipher->decrypt(income, decrypted);
			WAWO_ASSERT(decryptrt == wawo::OK);
			ctx->fire_read(decrypted);
		}

		int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) {
			WAWO_ASSERT(outlet != NULL);
			WAWO_ASSERT(m_cipher != NULL);

			WWRP<packet> encrypted;
			int rt = m_cipher->encrypt(outlet, encrypted);
			WAWO_ASSERT(rt == wawo::OK);

			return ctx->write(encrypted);
		}
	};
}}}
#endif