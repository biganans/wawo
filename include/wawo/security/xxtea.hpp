#ifndef _WAWO_SECURITY_XXTEA_HPP_
#define _WAWO_SECURITY_XXTEA_HPP_

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/security/cipher_abstract.hpp>

//borrow from: https://en.wikipedia.org/wiki/XXTEA
//published by David Wheeler and Roger Needham
#define WAWO_XXTEA_DELTA 0x9e3779b9
#define WAWO_XXTEA_MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

inline static void wikipedia_btea_encrypt(wawo::u32_t *v, wawo::u32_t const& n, wawo::u32_t key[4] ) {
	WAWO_ASSERT(sizeof(wawo::u32_t) == 4);
	wawo::u32_t y, z, sum;
	unsigned p, rounds, e;
	rounds = 6 + 52 / n;
	sum = 0;
	z = v[n - 1];
	do {
		sum += WAWO_XXTEA_DELTA;
		e = (sum >> 2) & 3;
		for (p = 0; p<n - 1; p++) {
			y = v[p + 1];
			z = v[p] += WAWO_XXTEA_MX;
		}
		y = v[0];
		z = v[n - 1] += WAWO_XXTEA_MX;
	} while (--rounds);
}
inline static void wikipedia_btea_decrypt(wawo::u32_t *v, wawo::u32_t const& n, wawo::u32_t key[4]) {
	WAWO_ASSERT(sizeof(wawo::u32_t) == 4);
	wawo::u32_t y, z, sum;
	unsigned p, rounds, e;
	rounds = 6 + 52 / n;
	sum = rounds*WAWO_XXTEA_DELTA;
	y = v[0];
	do {
		e = (sum >> 2) & 3;
		for (p = n - 1; p>0; p--) {
			z = v[p - 1];
			y = v[p] -= WAWO_XXTEA_MX;
		}
		z = v[n - 1];
		y = v[0] -= WAWO_XXTEA_MX;
		sum -= WAWO_XXTEA_DELTA;
	} while (--rounds);
}

inline static void xxtea_to_bytes_array(wawo::byte_t* arr, wawo::u32_t const& alength, wawo::u32_t const* const data, wawo::u32_t const& dlen) {
	WAWO_ASSERT(arr != NULL);
	WAWO_ASSERT(data != NULL);
	WAWO_ASSERT((dlen>>2)<=alength);

	if (wawo::is_little_endian()) {
		memcpy(arr, (wawo::byte_t*)data, alength);
		return;
	}
	for (wawo::u32_t i = 0; i < alength; ++i) {
		arr[i] = (wawo::byte_t)(data[i >> 2] >> ((i & 3) << 3));
	}

	(void)dlen;
	(void)alength;
}

inline static void xxtea_to_uint32_array(wawo::u32_t* arr, wawo::u32_t const& alength, wawo::byte_t const* const data, wawo::u32_t const& dlen) {
	WAWO_ASSERT(arr != NULL);
	WAWO_ASSERT(alength >= dlen >> 2);
	WAWO_ASSERT(data != NULL);
	if (wawo::is_little_endian()) {
		memcpy(arr, data, dlen);
		return;
	}
	for (wawo::u32_t i = 0; i < dlen; ++i) {
		arr[i >> 2] |= (wawo::u32_t)data[i] << ((i & 3) << 3);
	}
	(void)alength;
}

namespace wawo { namespace security {

	struct xxtea_ctx {
		byte_t* encrypted_data;
		u32_t encrypted_data_length;
		byte_t* decrypted_data;
		u32_t decrypted_data_length;
	};

	//different endian on both side is supported
	inline static xxtea_ctx* xxtea_encrypt( byte_t const* const data, u32_t const& dlen, byte_t const* const key, u32_t const& klen,int& ec ) {
		WAWO_ASSERT(key != NULL);
		WAWO_ASSERT(klen > 0);

		ec = wawo::OK;
		u32_t u32k[4] = {0};
		byte_t tmp[16] = {0};
		::memcpy(tmp, key, WAWO_MIN(klen, sizeof(u32k)));

		xxtea_to_uint32_array(u32k,4,tmp,16);

		u32_t u32len = ((dlen & 3) == 0) ? (dlen >> 2) : ((dlen >> 2) + 1);
		u32_t* u32data = (u32_t*)::calloc( sizeof(u32_t),(u32len +1)) ;
		if (u32data == NULL) {
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			return NULL;
		}
		*(u32data + u32len) = dlen;
		xxtea_to_uint32_array(u32data,u32len,data,dlen);
		wikipedia_btea_encrypt(u32data, u32len+1, u32k);

		xxtea_ctx* ctx = (xxtea_ctx*)::calloc(sizeof(xxtea_ctx), 1);
		if (ctx == NULL) {
			::free(u32data);
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			return NULL;
		}

		ctx->encrypted_data_length = (u32len + 1) << 2;
		ctx->encrypted_data = (byte_t*)calloc(sizeof(byte_t), ctx->encrypted_data_length);
		if (ctx->encrypted_data == NULL) {
			::free(u32data);
			::free(ctx);
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			return NULL;
		}

		xxtea_to_bytes_array(ctx->encrypted_data,ctx->encrypted_data_length,u32data,u32len);
		::free(u32data);
		return ctx;
	}

	inline static xxtea_ctx* xxtea_decrypt(byte_t const* const data, u32_t const& dlen, byte_t const* const key, u32_t const& klen, int& ec) {
		WAWO_ASSERT(key != NULL);
		WAWO_ASSERT(klen > 0);

		ec = wawo::OK;
		//invalid length check
		if ((dlen&3) != 0) {
			ec = wawo::E_XXTEA_INVALID_DATA;
			return NULL;
		}

		u32_t u32k[4] = {0};
		byte_t tmp[16] = {0};
		::memcpy(tmp, key, WAWO_MIN(klen, sizeof(u32k)));
		xxtea_to_uint32_array(u32k, 4, tmp, 16);

		u32_t u32len = dlen >> 2;
		u32_t* u32data = (u32_t*)calloc(sizeof(u32_t), u32len);
		if (u32data == NULL) {
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			return NULL;
		}
		xxtea_to_uint32_array(u32data,u32len,data,dlen);
		wikipedia_btea_decrypt(u32data, u32len, u32k);

		if (*(u32data + u32len - 1) > dlen) {
			//invalid data
			::free(u32data);
			ec = wawo::E_XXTEA_INVALID_DATA;
			return NULL;
		}

		xxtea_ctx* ctx = (xxtea_ctx*)calloc(sizeof(xxtea_ctx), 1);
		if (ctx == NULL) {
			::free(u32data);
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			return NULL;
		}

		ctx->decrypted_data_length = *(u32data + u32len - 1);
		ctx->decrypted_data = (byte_t*)calloc(sizeof(byte_t), ctx->decrypted_data_length);
		if (ctx->decrypted_data == NULL) {
			ec = wawo::E_MEMORY_ALLOC_FAILED;
			::free(ctx);
			::free(u32data);
			return NULL;
		}

		xxtea_to_bytes_array(ctx->decrypted_data, ctx->decrypted_data_length, u32data, (u32len-1) );
		::free(u32data);
		return	ctx;
	}

	inline static void xxtea_free(xxtea_ctx* ctx) {
		WAWO_ASSERT( ctx != NULL );
		if (ctx->encrypted_data) {
			::free(ctx->encrypted_data);
		}
		if (ctx->decrypted_data) {
			::free(ctx->decrypted_data);
		}
		::free(ctx);
	}

//#define WAWO_DEBUG_DH_XXTEA_KEY

	class xxtea:
		public cipher_abstract
	{

	private:
		byte_t m_key[16];

	public:
		xxtea()
		{
			byte_t k[16] = { 0 };
			set_key( k, 16);
		}
		xxtea( byte_t* k, u32_t const& klen ) {
			set_key(k,klen);
		}
		~xxtea() {}

		void set_key(byte_t const* const k, u32_t const& klen) {
			WAWO_ASSERT( k != NULL );
			WAWO_ASSERT( klen > 0 );
			::memcpy(m_key, k, klen );
			::memset(m_key + klen, 'z', sizeof(m_key) - klen);
		}

		int encrypt(WWSP<packet> const& in, WWSP<packet>& out ) const {
#ifdef WAWO_DEBUG_DH_XXTEA_KEY
			char key_cstr[64] = { 0 };
			snprintf(key_cstr, 64, "%d%d%d%d-%d%d%d%d-%d%d%d%d-%d%d%d%d"
				, m_key[0], m_key[1], m_key[2], m_key[3]
				, m_key[4], m_key[5], m_key[6], m_key[7]
				, m_key[8], m_key[9], m_key[10], m_key[11]
				, m_key[12], m_key[13], m_key[14], m_key[15]
			);
			WAWO_DEBUG("[xxtea]encrypt using key: %s", key_cstr );
#endif
			int ec;
			xxtea_ctx* tea = xxtea_encrypt(in->begin(), in->len(), m_key, sizeof(m_key)/sizeof(m_key[0]), ec );
			WAWO_ASSERT(ec == wawo::OK);
			if (tea == NULL) {
				return wawo::E_XXTEA_ENCRYPT_FAILED;
			}

			WWSP<packet> encrypted = wawo::make_shared<packet>(tea->encrypted_data_length);
			encrypted->write(tea->encrypted_data, tea->encrypted_data_length);
			xxtea_free(tea);
			out.swap(encrypted);

			return ec;
		}

		int decrypt(WWSP<packet> const& in, WWSP<packet>& out ) const {
#ifdef WAWO_DEBUG_DH_XXTEA_KEY
			char key_cstr[64] = { 0 };
			snprintf(key_cstr, 64, "%d%d%d%d-%d%d%d%d-%d%d%d%d-%d%d%d%d"
				, m_key[0], m_key[1], m_key[2], m_key[3]
				, m_key[4], m_key[5], m_key[6], m_key[7]
				, m_key[8], m_key[9], m_key[10], m_key[11]
				, m_key[12], m_key[13], m_key[14], m_key[15]
			);
			WAWO_DEBUG("[xxtea]decrypt using key: %s", key_cstr);
#endif
			int ec;
			xxtea_ctx* tea = xxtea_decrypt(in->begin(), in->len(), m_key, sizeof(m_key)/sizeof(m_key[0]), ec );
			WAWO_ASSERT(ec == wawo::OK);
			if (tea == NULL) {
				return wawo::E_XXTEA_DECRYPT_FAILED;
			}

			WWSP<packet> decrypted = wawo::make_shared<packet>(tea->decrypted_data_length);
			decrypted->write(tea->decrypted_data, tea->decrypted_data_length);
			xxtea_free(tea);
			out.swap(decrypted);

			return ec;
		}
	};
}}
#endif