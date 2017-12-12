#ifndef _WAWO_SECURITY_CIPHER_ABSTRACT_HPP
#define _WAWO_SECURITY_CIPHER_ABSTRACT_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace security {

	class cipher_abstract
	{
	public:
		cipher_abstract() {}
		virtual ~cipher_abstract() {}

		virtual void set_key(byte_t const* const k, u32_t const& klen) = 0;
		virtual int encrypt(WWSP<packet> const& in, WWSP<packet>& out) const = 0;
		virtual int decrypt(WWSP<packet> const& in, WWSP<packet>& out) const = 0;
	};
}}
#endif