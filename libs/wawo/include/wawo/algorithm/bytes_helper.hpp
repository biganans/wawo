#ifndef _WAWO_ALGORITHM_BYTES_HELPER_HPP_
#define _WAWO_ALGORITHM_BYTES_HELPER_HPP_

#include <wawo/core.h>
#include <string>

namespace wawo { namespace algorithm {

	namespace bigendian_bytes_helper {
	
		template <class T> struct type{};

		template <class T, class InIt>
		inline T read_impl( InIt const& start , type<T> ) {
			T ret = 0;
			int idx = 0;

			for( int i=0;i<sizeof(T);i++ ) {
				ret <<= 8;
				ret |= static_cast<uint8_t>( *(start + idx++) );
			}

			return ret;
		}

		template <class InIt>
		inline uint8_t read_uint8( InIt const& start ) {
			return static_cast<uint8_t>( *start );
		}
		template <class InIt>
		inline int8_t read_int8( InIt const & start ) {
			return static_cast<int8_t>( *start );
		}

		template <class InIt>
		inline uint16_t read_uint16( InIt const& start ) {
			return read_impl( start, type<uint16_t>() );
		}
		template <class InIt>
		inline int16_t read_int16( InIt const& start ) {
			return read_impl( start, type<int16_t>() );
		}

		template <class InIt>
		inline uint32_t read_uint32( InIt const& start) {
			return read_impl( start, type<uint32_t>() );
		}
		template <class InIt>
		inline int32_t read_int32( InIt const& start ) {
			return read_impl( start, type<int32_t>() ) ;
		}

		template <class InIt>
		inline uint64_t read_uint64( InIt const& start) {
			return read_impl( start, type<uint64_t>() );
		}
		template <class InIt>
		inline int64_t read_int64( InIt const& start ) {
			return read_impl( start, type<int64_t>() );
		}

		template <class InIt>
		inline uint32_t read_bytes( byte_t* const target, uint32_t const& length,InIt const& start ) {
			wawo::uint32_t read_idx = 0;
			
			for( ; read_idx < length;read_idx ++ ) {
				*(target+read_idx) = *(start+read_idx) ;
			}

			return read_idx ;
		}

		template <class T, class OutIt>
		inline wawo::uint32_t write_impl( T const& val, OutIt const& start_addr ) {
		
			wawo::uint32_t write_idx = 0;
			for( int i=(sizeof(T)-1); i>=0; --i ) {
				*(start_addr+write_idx++) = ( (val>>(i*8)) & 0xff );
			}

			return write_idx;
		}

		template <class OutIt>
		inline wawo::uint32_t write_uint8( uint8_t const& val , OutIt const& out ) {
			return write_impl( val, out );
		}

		template <class OutIt>
		inline wawo::uint32_t write_int8( int8_t const& val, OutIt const& out) {
			return write_impl( val, out );
		}

		template <class OutIt>
		inline wawo::uint32_t write_uint16(uint16_t const& val , OutIt const& out) {
			return write_impl( val, out );
		}
		template <class OutIt>
		inline wawo::uint32_t write_int16(int16_t const& val , OutIt const& out) {
			return write_impl( val, out );
		}

		template <class OutIt>
		inline wawo::uint32_t write_uint32(uint32_t const& val, OutIt const& out) {
			return write_impl(val, out);
		}
		template <class OutIt>
		inline wawo::uint32_t write_int32(int32_t const& val, OutIt const& out) {
			return write_impl(val, out);
		}

		template <class OutIt>
		inline wawo::uint32_t write_uint64(uint64_t const& val, OutIt const& out) {
			return write_impl(val, out);
		}
		template <class OutIt>
		inline wawo::uint32_t write_int64(int64_t const& val, OutIt const& out) {
			return write_impl(val, out);
		}

		template <class OutIt>
		inline wawo::uint32_t write_bytes( const byte_t* const buffer, wawo::uint32_t const& length, OutIt const& out ) {

			wawo::uint32_t write_idx = 0;
			for( ; write_idx < length;write_idx ++ ) {
				*(out+write_idx) = *(buffer+write_idx) ; 
			}

			return write_idx;
		}

		inline wawo::uint32_t write_string( std::string const& val, char*& out ) {
			std::memcpy( (void*)out,  val.c_str(), val.size() ) ;
//			out += val.size();

			return val.size();
		}

		template <class OutIt>
		wawo::uint32_t write_string(std::string const& val, OutIt const& out) {
		
			wawo::uint32_t write_idx = 0;
			for (std::string::const_iterator i=val.begin()
				,end(val.end()); i!= end; ++i ) {
				*(out + write_idx++) = *i;
			}

			return write_idx ;
		}
	}

	namespace bytes_helper = bigendian_bytes_helper ;
}}

#endif