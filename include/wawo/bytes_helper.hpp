#ifndef _WAWO_BYTES_HELPER_HPP_
#define _WAWO_BYTES_HELPER_HPP_

#include <wawo/core.hpp>

namespace wawo {

	namespace bytes_helper {
		//big endian 0x01020304 => 01 02 03 04 (most significant first)		
		template <class T> struct type{};

		struct big_endian {
			template <class T, class InIt>
			static inline T read_impl(InIt const& start, type<T>) {
				T ret = 0;
				for (::size_t i = 0; i < sizeof(T); ++i) {
					ret <<= 8;
					ret |= static_cast<u8_t>(*(start + i));
				}
				return ret;
			}
			template <class T, class OutIt>
			static inline wawo::u32_t write_impl(T const& val, OutIt const& start_addr) {
				wawo::u32_t write_idx = 0;
				for (::size_t i = (sizeof(T) - 1); i != ~0; --i) {
					*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				}
				return write_idx;
			}
		};

		struct little_endian {
			template <class T, class InIt>
			static inline T read_impl(InIt const& start, type<T>) {
				T ret = 0;
				for (::size_t i = (sizeof(T) - 1); i != ~0; --i) {
					ret <<= 8;
					ret |= static_cast<u8_t>(*(start + i));
				}
				return ret;
			}

			template <class T, class OutIt>
			static inline wawo::u32_t write_impl(T const& val, OutIt const& start_addr) {
				wawo::u32_t write_idx = 0;
				for (::size_t i = 0 ; i <sizeof(T); ++i) {
					*(start_addr + write_idx++) = ((val >> (i * 8)) & 0xff);
				}
				return write_idx;
			}
		};

		template <class InIt>
		inline u8_t read_u8( InIt const& start ) {
			return static_cast<u8_t>( *start );
		}
		template <class InIt>
		inline i8_t read_i8( InIt const & start ) {
			return static_cast<i8_t>( *start );
		}
		template <class InIt, class endian=big_endian>
		inline u16_t read_u16( InIt const& start ) {
			return endian::read_impl( start, type<u16_t>() );
		}
		template <class InIt, class endian = big_endian>
		inline i16_t read_i16( InIt const& start ) {
			return endian::read_impl( start, type<i16_t>() );
		}

		template <class InIt, class endian = big_endian>
		inline u32_t read_u32( InIt const& start) {
			return endian::read_impl( start, type<u32_t>() );
		}
		template <class InIt, class endian = big_endian>
		inline i32_t read_i32( InIt const& start ) {
			return endian::read_impl( start, type<i32_t>() ) ;
		}

		template <class InIt, class endian = big_endian>
		inline u64_t read_u64( InIt const& start) {
			return endian::read_impl( start, type<u64_t>() );
		}
		template <class InIt, class endian = big_endian>
		inline i64_t read_i64( InIt const& start ) {
			return endian::read_impl( start, type<i64_t>() );
		}

		template <class InIt>
		inline wawo::u32_t read_bytes( byte_t* const target, wawo::u32_t const& len,InIt const& start ) {
			wawo::u32_t read_idx = 0;
			for( ; read_idx < len; ++read_idx ) {
				*(target+read_idx) = *(start+read_idx) ;
			}
			return read_idx ;
		}

		template <class OutIt>
		inline wawo::u32_t write_u8( u8_t const& val , OutIt const& out ) {
			return write_impl( val, out );
		}

		template <class OutIt>
		inline wawo::u32_t write_i8( i8_t const& val, OutIt const& out) {
			return write_impl( val, out );
		}

		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_u16(u16_t const& val , OutIt const& out) {
			return endian::write_impl( val, out );
		}
		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_i16(i16_t const& val , OutIt const& out) {
			return endian::write_impl( val, out );
		}

		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_u32(u32_t const& val, OutIt const& out) {
			return endian::write_impl(val, out);
		}
		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_i32(i32_t const& val, OutIt const& out) {
			return endian::write_impl(val, out);
		}

		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_u64(u64_t const& val, OutIt const& out) {
			return endian::write_impl(val, out);
		}
		template <class OutIt, class endian = big_endian>
		inline wawo::u32_t write_i64(i64_t const& val, OutIt const& out) {
			return endian::write_impl(val, out);
		}

		template <class OutIt>
		inline wawo::u32_t write_bytes( const byte_t* const buffer, wawo::u32_t const& len, OutIt const& out ) {
			wawo::u32_t write_idx = 0;
			for( ; write_idx < len;++write_idx ) {
				*(out+write_idx) = *(buffer+write_idx) ;
			}
			return write_idx;
		}
	}
}
#endif