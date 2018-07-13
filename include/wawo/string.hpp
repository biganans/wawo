#ifndef _WAWO_STRING_HPP_
#define _WAWO_STRING_HPP_

#include <vector>
#include <cstring>
#include <cwchar>
#include <algorithm>

#include <wawo/config/platform.h>
#include <wawo/macros.h>
#include <wawo/memory.hpp>

#define _WAWO_IS_CHAR_T(_T) (sizeof(_T)==sizeof(char))
#define _WAWO_IS_WCHAR_T(_T) (sizeof(_T)==sizeof(wchar_t))

//extern part
namespace wawo {

#if defined(WAWO_PLATFORM_WIN) && _MSC_VER <= 1700
	/* abc-333		-> -333
	 * abc333		-> +333
	 * abc 333		-> +333
	 * abc -333		-> -333
	 * abc -3 33	-> -3
	 * abc -3abc333	-> -3
	 */

	inline i64_t to_i64(char const* const str) {
		if( str == NULL ) return 0;
		i64_t ret = 0;
		int i = 0;
		int negated_flag = 1;

		while( *(str+i) != '\0') {
			if( (*(str+i) <= '9' && *(str+i) >= '0') || *(str+i) == '-' || *(str+i) == '+' )
				break;
			++i;
		}

		if( *(str+i) == '-' ) {
			negated_flag = -1;
			++i;
		}

		if( *(str+i) == '+' ) {
			negated_flag = 1;
			++i;
		}

		while( *(str+i) != '\0' ) {
			if( !(*(str+i) >= '0' && *(str+i)<='9') )
			{
				break;
			}
			ret *= 10;
			ret += (*(str+i)) - '0';
			++i;
		}
		return ret*negated_flag;
	}

	inline u64_t to_u64(char const* const str) {
		if( str == NULL ) return 0;
		u64_t ret = 0;
		int i = 0;
		int negated_flag = 1;

		while( *(str+i) != '\0') {
			if( (*(str+i) <= '9' && *(str+i) >= '0') || *(str+i) == '-' || *(str+i) == '+' )
				break;
			++i;
		}

		if( *(str+i) == '-' ) {
			negated_flag = -1;
			++i;
		}

		if( *(str+i) == '+' ) {
			negated_flag = 1;
			++i;
		}

		while( *(str+i) != '\0' ) {
			if( !(*(str+i) >= '0' && *(str+i)<='9') )
			{
				break;
			}
			ret *= 10;
			ret += (*(str+i)) - '0';
			++i;
		}
		return ret*negated_flag;
	}
#else
	inline i64_t to_i64(char const* const str) {
		WAWO_ASSERT(str != NULL);
		char* pEnd;
		return std::strtoll(str, &pEnd, 10);
	}

	inline u64_t to_u64(char const* const str) {
		WAWO_ASSERT(str != NULL);
		char* pEnd;
		return std::strtoull(str, &pEnd, 10);
	}
#endif

	inline i32_t to_i32(char const* const str) {
		char* pEnd;
		return std::strtol(str, &pEnd, 10);
	}
	inline u32_t to_u32(char const* const str) {
		char* pEnd;
		return std::strtoul(str, &pEnd, 10);
	}

	//typedef u32_t size_t;

	/* Please read these lines first if u have any question on these string apis
	 * why We have these APIs ?
	 *
	 * 1, std::strcpy is not safe
	 * 2, strcpy_s and strlcpy's parameter sequence is different
	 * 3, we don't have a wcsncpy_s on gnu
	 * 4, there would be no obvious performance difference if we imply it by memxxx APIs
	 * 5, if I'm wrong, please correct me, thx.
	 */

	inline wawo::size_t strlen( char const* const str ) {
		return std::strlen(str);
	}

	inline wawo::size_t strlen(wchar_t const* const wstr ) {
		return std::wcslen(wstr);
	}

	inline int strcmp(char const* const lcptr, char const* const rcptr ) {
		return std::strcmp(lcptr,rcptr);
	}
	inline int strncmp(char const* const lcptr, char const* const rcptr, wawo::size_t const& length ) {
		return std::strncmp(lcptr,rcptr,length);
	}

	inline int strcmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr ) {
		return std::wcscmp(lwcptr,rwcptr);
	}

	inline int strncmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr, wawo::size_t const& length) {
		return std::wcsncmp(lwcptr,rwcptr,length);
	}

	//src must be null-terminated
	//The behavior is undefined if the dest array is not large enough. The behavior is undefined if the strings overlap
	inline char* strcpy(char* const dst, char const* const src) {
		wawo::size_t src_len = wawo::strlen(src);
		::memcpy(dst,src,src_len);
		*(dst+src_len) = '\0';
		return dst;
	}
	inline wchar_t* strcpy(wchar_t* const dst, wchar_t const* const src) {
		wawo::size_t src_len = wawo::strlen(src);
		::memcpy(dst,src,src_len*sizeof(wchar_t) );
		*(dst+src_len) = L'\0';
		return dst;
	}

	inline char* strncpy(char* const dst, char const* const src, wawo::size_t const& length) {
		wawo::size_t src_len = wawo::strlen(src);
		wawo::size_t copy_length = (src_len>length)?length:src_len;
		::memcpy(dst,src,copy_length*sizeof(char));
		*(dst+length) = '\0';
		return dst;
	}
	inline wchar_t* strncpy(wchar_t* const dst, wchar_t const* const src, wawo::size_t const& length) {
		wawo::size_t src_len = wawo::strlen(src);
		wawo::size_t copy_length = (src_len>length)?length:src_len;
		::memcpy(dst,src,copy_length*sizeof(wchar_t));
		*(dst+length) = L'\0';
		return dst;
	}

	//src must be null-terminated, or the behaviour is undefined
	//dst must have enough space, or the behaviour is undefined if the strings overlap
	//dst will be null-terminated after strcat&strncat
	inline char* strcat(char* const dst, char const* const src) {
		wawo::size_t dst_length = wawo::strlen(dst);
		wawo::size_t src_len = wawo::strlen(src);

		wawo::strncpy(dst+dst_length,src,src_len);
		return dst;
	}
	inline char* strncat(char* const dst, char const* const src, wawo::size_t const& length) {
		wawo::size_t dst_length = wawo::strlen(dst);
		wawo::size_t src_len = wawo::strlen(src);
		wawo::size_t copy_length = (src_len>length) ? length : src_len;

		wawo::strncpy(dst+dst_length,src,copy_length);
		return dst;
	}
	inline wchar_t* strncat(wchar_t* const dst, wchar_t const* const src, wawo::size_t const& length) {
		wawo::size_t dst_length = wawo::strlen(dst);
		wawo::size_t src_len = wawo::strlen(src);
		wawo::size_t copy_length = (src_len>length) ? length : src_len;

		wawo::strncpy(dst+dst_length,src,copy_length);
		return dst;
	}

	inline char* strstr(char* const dst, char const* const check) {
		return std::strstr( dst, check);
	}
	inline char const* strstr( char const* const dst, char const* const check ) {
		return std::strstr(dst,check);
	}

	inline wchar_t* strstr(wchar_t const* const dst, wchar_t const* const check) {
		return std::wcsstr( const_cast<wchar_t*>(dst), check);
	}
	inline wchar_t const* strstr(wchar_t* const dst, wchar_t const* const check) {
		return std::wcsstr( dst, check);
	}

	//return the first occurence of the string's appear postion
	//not the bytes position , but the char|wchar_t 's sequence order number
	inline int strpos( char const* const dst, char const* const check ) {
		char const* sub = wawo::strstr(dst,check);

		if(sub==NULL) {
			return -1;
		}

		return wawo::long_t(sub-dst);
	}

	inline int strpos( wchar_t const* const dst, wchar_t const* const check ) {
		wchar_t const* sub = wawo::strstr(dst,check);

		if(sub==NULL) {
			return -1;
		}

		return wawo::long_t(sub-dst);
	}

	inline void strtolower( char* const buffer, u32_t const& size, char const* const src ) {
		WAWO_ASSERT(size>=wawo::strlen(src) );
		wawo::size_t c = wawo::strlen(src);
		wawo::size_t i = 0;
		while ( (i<c) && (i<size) )  {
			*(buffer + i) = static_cast<char>(::tolower(*(src + i) ));
			++i;
		}
	}

	inline void strtoupper(char* const buffer, wawo::size_t const& size, char const* const src) {
		WAWO_ASSERT(size >= wawo::strlen(src));
		wawo::size_t c = wawo::strlen(src);
		wawo::size_t i = 0;
		while ( (i<c) && (i<size)) {
			*(buffer + i) = static_cast<char>(::toupper(*(src + i)));
			++i;
		}
	}

	//enum CharacterSet
	//{
	//	CS_UTF8,
	//	CS_GBK,
	//	CS_GB2312,
	//	CS_BIG5
	//};

	//wctomb,wcstombs,mbstowcs,setlocal
}

namespace wawo {

	template <typename _char_t>
	struct len_cstr_impl {
		//supported operation
		// 1, new & new with a init length
		// 2, init assign
		// 3, copy assign
		// 4, operator + for concat,
		// 5, length of string
		// 6, cstr address of string
		// 7, for any other requirement , please use std::string|std::wstring,,,
		typedef _char_t char_t;
		typedef _char_t* char_t_ptr;
		typedef len_cstr_impl<char_t> _MyT;

		char_t* cstr;
		wawo::size_t len;

		len_cstr_impl():
			cstr( (char_t*)("")),
			len(0)
		{
		}

		len_cstr_impl( char_t const* const str):
			cstr( (char_t*)("")),
			len(0)
		{
			WAWO_ASSERT( str != NULL );
			len = wawo::strlen(str) ;// str + '\0'

			if(len>0 ) {
				WAWO_ASSERT( cstr != str );
				cstr = (char_t*)::malloc( sizeof(char_t)*(len+1) );
				WAWO_ALLOC_CHECK(cstr, sizeof(char_t)*(len + 1));
				wawo::strcpy(cstr,str);
			}
		}

		explicit len_cstr_impl( char_t const* const str, wawo::size_t const& slen ) :
			cstr( (char_t*)("")),
			len(0)
		{
			WAWO_ASSERT( wawo::strlen(str) >= slen );
			if(slen > 0 ) {
				len = slen;
				cstr = (char_t*)::malloc( sizeof(char)*(slen +1) ); // str + '\0'
				WAWO_ALLOC_CHECK(cstr, sizeof(char)*(slen + 1));
				wawo::strncpy(cstr,str, slen);
			}
		}

		~len_cstr_impl() {
			if(len>0){
				::free(cstr);
			}
			cstr = NULL;
			len =0;
		}

		len_cstr_impl substr(wawo::size_t const& begin, wawo::size_t const& _len ) const {
			WAWO_ASSERT( ( begin + _len) <= len );
			return len_cstr_impl( cstr+begin, _len );
		}

		void swap(len_cstr_impl& other ) {
			std::swap( cstr,other.cstr );
			std::swap( len, other.len );
		}

		len_cstr_impl(len_cstr_impl const& other ) :
			cstr( (char_t*)("")),
			len(0)
		{
			if( other.len > 0 ) {
				cstr = (char_t*)::malloc( sizeof(char_t)*(other.len +1) );
				WAWO_ALLOC_CHECK(cstr, sizeof(char_t)*(other.len + 1));

				len = other.len;
				//contain '\0'
				::memcpy(cstr,other.cstr,(len +1)*sizeof(char_t) );
			}
		}

		len_cstr_impl& operator = (len_cstr_impl const& other) {
			len_cstr_impl(other).swap( *this );
			return *this;
		}

		//str must be null terminated, or behaviour undefined
		len_cstr_impl& operator = ( char const* const str ) {
			len_cstr_impl(str).swap(*this);
			return *this;
		}

		inline bool operator == (len_cstr_impl const& other) const {
			return (len == other.len) && (wawo::strncmp( cstr,other.cstr, len) == 0) ;
		}
		inline bool operator != (len_cstr_impl const& other) const {
			return (len != other.len) || (wawo::strncmp( cstr,other.cstr, len) != 0) ;
		}

		inline bool operator < (len_cstr_impl const& other) const {
			return wawo::strcmp(cstr, other.cstr) < 0;
		}

		inline bool operator > (len_cstr_impl const& other) const {
			return wawo::strcmp(cstr, other.cstr) > 0;
		}

		_MyT operator + (len_cstr_impl const& other) const {
			wawo::size_t buffer_length = len + other.len + 1;
			char_t* _buffer = (char_t*) ::calloc( sizeof(char_t), buffer_length );

			if( len ) {
				wawo::strncpy( _buffer, cstr, len );
			}

			if( other.len ) {
				wawo::strncat( _buffer, other.cstr, other.len );
			}

			_MyT lcstr(_buffer, len + other.len );
			::free(_buffer);

			return lcstr;
		}

		_MyT operator + ( char const* const str ) const {
			return *this + len_cstr_impl( str );
		}

		_MyT& operator += (len_cstr_impl const& other ) {
			_MyT tmp = (*this) + other;
			tmp.swap(*this);
			return *this;
		}

		_MyT& operator += (char const* const str) {
			_MyT tmp = (*this) + str;
			tmp.swap(*this);
			return *this;
		}
	};

	typedef len_cstr_impl<char>		len_cstr;
	typedef len_cstr_impl<wchar_t>	len_wcstr;

	inline len_cstr	to_string(i64_t const& int64) {
		char tmp[32] = {0};
		snprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), "%lld", int64 );
		return len_cstr(tmp,wawo::strlen(tmp));
	}
	inline len_cstr	to_string(u64_t const& uint64) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%llu", uint64);
		return len_cstr(tmp, wawo::strlen(tmp));
	}
	inline len_cstr	to_string(i32_t const& int32) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%d", int32);
		return len_cstr(tmp, wawo::strlen(tmp));
	}
	inline len_cstr	to_string(u32_t const& uint32) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%u", uint32);
		return len_cstr(tmp, wawo::strlen(tmp));
	}

	inline wawo::len_cstr rtrim(wawo::len_cstr const& source_) {
		if (source_.len == 0) return wawo::len_cstr();

		wawo::size_t end = source_.len - 1;
		while (end > 0 && (*(source_.cstr + end)) == ' ')
			--end;

		return wawo::len_cstr( source_.cstr, end + 1 );
	}

	inline wawo::len_cstr ltrim(wawo::len_cstr const& source_) {
		if (source_.len == 0) return wawo::len_cstr();

		wawo::size_t start = 0;
		while (start < source_.len && (*(source_.cstr + start)) == ' ')
			++start;

		return wawo::len_cstr(source_.cstr + start, source_.len - start);
	}

	inline wawo::len_cstr trim(wawo::len_cstr const& source_) {
		return rtrim(ltrim(source_));
	}

	//return n if replace success, otherwise return -1
	inline int replace(wawo::len_cstr const& source_, wawo::len_cstr const& search_, wawo::len_cstr const& replace_, wawo::len_cstr& result) {
		int replace_count = 0;
		wawo::len_cstr source = source_;
	begin:
		int pos = wawo::strpos(source.cstr, search_.cstr);
		if (pos == -1) {
			return replace_count;
		}
		wawo::len_cstr new_str = source.substr(0, pos) + replace_ + source.substr(pos + search_.len, source.len - (pos + search_.len));
		++replace_count;
		source = new_str;

		result = new_str;
		goto begin;
	}

	inline void split(std::string const& string, std::string const& delimiter, std::vector<std::string>& result ) {

		WAWO_ASSERT( result.size() == 0 );
		char const* check_cstr = string.c_str();
		wawo::size_t check_length = string.length();
		wawo::size_t next_check_idx = 0;

		char const* delimiter_cstr = delimiter.c_str();
		wawo::size_t delimiter_length = delimiter.length();

		int hit_pos ;

		while( (next_check_idx < check_length) && ( hit_pos = wawo::strpos( check_cstr + next_check_idx, delimiter_cstr )) != -1 )
		{
			std::string tmp( (check_cstr + next_check_idx), hit_pos );
			next_check_idx += (hit_pos + delimiter_length);

			if( tmp.length() > 0 ) {
				result.push_back( tmp );
			}
		}

		if( (check_length - next_check_idx) > 0 ) {
			std::string left_string( (check_cstr + next_check_idx), check_length-next_check_idx );
			result.push_back( left_string );
		}
	}
	
	inline void split( wawo::len_cstr const& lcstr, wawo::len_cstr const& delimiter_string, std::vector<wawo::len_cstr>& result ) {
		std::vector<std::string> length_result;
		split( std::string(lcstr.cstr, lcstr.len ), std::string(delimiter_string.cstr, delimiter_string.len), length_result);

		std::for_each(length_result.begin(), length_result.end(), [&result]( std::string const& lengthcstr ) {
			result.push_back( wawo::len_cstr( lengthcstr.c_str(), lengthcstr.length()) );
		});
	}

	inline void join( std::vector<wawo::len_cstr> const& strings, wawo::len_cstr const& delimiter, wawo::len_cstr& result ) {
		if( strings.size() == 0 )
		{
			return ;
		}

		u32_t curr_idx = 0;
		result += strings[curr_idx];
		curr_idx++;

		while( curr_idx < strings.size() ) {
			result += delimiter;
			result += strings[curr_idx++];
		}
	}

	inline void join( std::vector<std::string> const& strings, std::string const& delimiter, std::string& result ) {
		std::vector<len_cstr> length_cstr_vector;

		std::for_each( strings.begin(), strings.end(), [&length_cstr_vector]( std::string const& sstr ) {
			length_cstr_vector.push_back( len_cstr( sstr.c_str(), sstr.length() ) );
		});

		len_cstr length_cstr_tmp;
		join( length_cstr_vector, len_cstr( delimiter.c_str(), delimiter.length()), length_cstr_tmp);
		result = std::string(length_cstr_tmp.cstr, length_cstr_tmp.len );
	}
}



namespace std {

	template <>
	struct hash<wawo::len_cstr>
	{
		size_t operator() ( wawo::len_cstr const& lenstr) const {
			static std::hash<std::string> std_string_hash_func;
			return std_string_hash_func(std::string(lenstr.cstr, lenstr.len));
		}
	};
}

#endif