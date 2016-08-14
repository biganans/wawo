#ifndef WAWO_SECURITY_OBFUSCATE_HPP
#define	WAWO_SECURITY_OBFUSCATE_HPP

#include <wawo/core.h>

//UINT_X's width must be less than sizeof(u32_t), it can be u8_t or , u16_t
template <typename UINT_X>
inline static wawo::u32_t obfuscate(UINT_X const& val, wawo::u32_t const& r_u32) {
	if (sizeof(wawo::u32_t) <= sizeof(UINT_X)) return val;
	wawo::u32_t rr = 0;
	wawo::u8_t b[sizeof(UINT_X) * 8];
	wawo::u8_t bii[sizeof(UINT_X) * 8];

	//per bytes contain bit count
	wawo::u8_t pbcbc = (sizeof(UINT_X) * 8) / (sizeof(wawo::u32_t) / sizeof(wawo::u8_t));

	for (int i = 0; i < (sizeof(UINT_X) * 8); i++) {
		if ((i%pbcbc) == 0) {
			b[i] = ((r_u32 >> ((5 + ((i / pbcbc) << 3))) & 0x7) % 5) & 0xFF;
			bii[i] = b[i];
		} else {
			b[i] = ((b[i - 1] + 1) % 5);
			bii[i] = bii[i - 1];
		}

		wawo::u8_t bi = 0;
		if (val&(0x01 << i)) {
			bi |= (0x01 << b[i]);
		}
		bi |= ((0x7 & bii[i]) << 5);
		rr |= ((0xFFFFFFFF & bi) << ((i / pbcbc) << 3));
	}
	return rr;
}

template <typename UINT_X>
inline static UINT_X deobfuscate(wawo::u32_t const& val) {
	UINT_X r = 0;
	wawo::u8_t pbcbc = (sizeof(UINT_X) * 8) / (sizeof(wawo::u32_t) / sizeof(wawo::u8_t));
	UINT_X b[sizeof(UINT_X) * 8];

	for (int i = 0; i < (sizeof(UINT_X) * 8); i++) {
		if ((i%pbcbc) == 0) {
			b[i] = ((val >> ((5 + ((i / pbcbc) << 3))) & 0x7) % 5) & 0xFF;
		} else {
			b[i] = ((b[i - 1] + 1) % 5);
		}
		r |= (((val >> (b[i] + ((i / pbcbc) << 3))) & 0x1) << i);
	}
	return r;
}

namespace wawo { namespace security {
	inline static u32_t u8_obfuscate(u8_t const& val, u32_t const& r_u32) {
		return obfuscate<u8_t>(val,r_u32);
	}
	inline static u32_t u16_obfuscate(u16_t const& val, u32_t const& r_u32) {
		return obfuscate<u16_t>(val, r_u32);
	}
	inline static u8_t u8_deobfuscate(u32_t const& val) {
		return deobfuscate<u8_t>(val);
	}
	inline static u16_t u16_deobfuscate(u32_t const& val) {
		return deobfuscate<u16_t>(val);
	}
}}
#endif