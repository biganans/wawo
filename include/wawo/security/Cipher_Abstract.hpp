#ifndef _WAWO_SECURITY_CIPHER_ABSTRACT_HPP
#define _WAWO_SECURITY_CIPHER_ABSTRACT_HPP

#include <wawo/core.h>
#include <wawo/algorithm/Packet.hpp>
namespace wawo { namespace security {

	using namespace wawo::algorithm;
	class Cipher_Abstract
	{
	public:
		Cipher_Abstract() {}
		virtual ~Cipher_Abstract() {}

		virtual void SetKey(byte_t const* const k, u32_t const& klen) = 0;
		virtual int Encrypt(WWSP<Packet> const& in, WWSP<Packet>& out) const = 0;
		virtual int Decrypt(WWSP<Packet> const& in, WWSP<Packet>& out) const = 0;
	};
}}
#endif