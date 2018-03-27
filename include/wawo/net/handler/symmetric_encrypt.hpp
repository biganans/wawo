#ifndef _WAWO_NET_HANDLER_SYMMETRIC_ENCRYPT_HPP
#define _WAWO_NET_HANDLER_SYMMETRIC_ENCRYPT_HPP

#include <wawo/core.hpp>
#include <wawo/net/socket_handler.hpp>

#include <wawo/security/cipher_abstract.hpp>

namespace wawo { namespace net { namespace handler {

	//packet based encrypt/decrypt
	class symmetric_encrypt :
		public wawo::net::socket_inbound_handler_abstract,
		public wawo::net::socket_outbound_handler_abstract
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

		void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income ) {
			WAWO_ASSERT(m_cipher != NULL);
			WWSP<packet> decrypted;
			int decryptrt = m_cipher->decrypt(income, decrypted);
			WAWO_ASSERT(decryptrt == wawo::OK);
			ctx->fire_read(decrypted);
		}

		void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) {
			WAWO_ASSERT(outlet != NULL);
			WAWO_ASSERT(m_cipher != NULL);

			WWSP<packet> encrypted;
			int rt = m_cipher->encrypt(outlet, encrypted);
			WAWO_ASSERT(rt == wawo::OK);

			ctx->write(encrypted);
		}
	};
}}}
#endif