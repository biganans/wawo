#ifndef _WAWO_SECURITY_DH_HPP_
#define _WAWO_SECURITY_DH_HPP_

#include <wawo/core.h>
#include <wawo/math/formula.hpp>

namespace wawo { namespace security {

	/*
	 * refer to
	 * https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange
	 *
	 */
	const static u64_t DH_q = 0xffffffffffffffc5ULL;
	const static u64_t DH_g = 5;

	struct DH_Factor {
		u64_t priv_key;
		u64_t pub_key;
	};

	inline static DH_Factor DH_GenerateFactor() {
		DH_Factor f = { 0,0 };
		f.priv_key = wawo::random_u64();
		f.pub_key = wawo::math::formula::Q_powmod(DH_g, f.priv_key, DH_q);
		return f;
	}
	inline static u64_t DH_GenerateKey(u64_t const& factor2_pub, u64_t const& factor1_priv) {
		return wawo::math::formula::Q_powmod(factor2_pub, factor1_priv, DH_q);
	}
}}
#endif