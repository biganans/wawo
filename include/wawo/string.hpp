#ifndef _WAWO_STRING_HPP_
#define _WAWO_STRING_HPP_

#include <cstring>
#include <cwchar>

#include <algorithm>
#include <vector>
#include <wawo/config/platform.h>

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

	inline u32_t strlen( char const* const str ) {
		return std::strlen(str);
	}

	inline u32_t strlen(wchar_t const* const wstr ) {
		return std::wcslen(wstr);
	}

	inline int strcmp(char const* const lcptr, char const* const rcptr ) {
		return std::strcmp(lcptr,rcptr);
	}
	inline int strncmp(char const* const lcptr, char const* const rcptr, u32_t const& len ) {
		return std::strncmp(lcptr,rcptr,len);
	}

	inline int strcmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr ) {
		return std::wcscmp(lwcptr,rwcptr);
	}

	inline int strncmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr, u32_t const& len) {
		return std::wcsncmp(lwcptr,rwcptr,len);
	}

	//src must be null-terminated
	//The behavior is undefined if the dest array is not large enough. The behavior is undefined if the strings overlap
	inline char* strcpy(char* const dst, char const* const src) {
		u32_t src_len = wawo::strlen(src);
		std::memcpy(dst,src,src_len);
		*(dst+src_len) = '\0';
		return dst;
	}
	inline wchar_t* strcpy(wchar_t* const dst, wchar_t const* const src) {
		u32_t src_len = wawo::strlen(src);
		std::memcpy(dst,src,src_len*sizeof(wchar_t) );
		*(dst+src_len) = L'\0';
		return dst;
	}

	inline char* strncpy(char* const dst, char const* const src, u32_t const& len) {
		u32_t src_len = wawo::strlen(src);
		u32_t copy_len = (src_len>len)?len:src_len;
		std::memcpy(dst,src,copy_len*sizeof(char));
		*(dst+len) = '\0';
		return dst;
	}
	inline wchar_t* strncpy(wchar_t* const dst, wchar_t const* const src, u32_t const& len) {
		u32_t src_len = wawo::strlen(src);
		u32_t copy_len = (src_len>len)?len:src_len;
		std::memcpy(dst,src,copy_len*sizeof(wchar_t));
		*(dst+len) = L'\0';
		return dst;
	}

	//src must be null-terminated, or the behaviour is undefined
	//dst must have enough space, or the behaviour is undefined if the strings overlap
	//dst will be null-terminated after strcat&strncat
	inline char* strcat(char* const dst, char const* const src) {
		u32_t dst_len = wawo::strlen(dst);
		u32_t src_len = wawo::strlen(src);

		wawo::strncpy(dst+dst_len,src,src_len);
		return dst;
	}
	inline char* strncat(char* const dst, char const* const src, u32_t const& len) {
		u32_t dst_len = wawo::strlen(dst);
		u32_t src_len = wawo::strlen(src);
		u32_t copy_len = (src_len>len) ? len : src_len;

		wawo::strncpy(dst+dst_len,src,copy_len);
		return dst;
	}
	inline wchar_t* strncat(wchar_t* const dst, wchar_t const* const src, u32_t const& len) {
		u32_t dst_len = wawo::strlen(dst);
		u32_t src_len = wawo::strlen(src);
		u32_t copy_len = (src_len>len) ? len : src_len;

		wawo::strncpy(dst+dst_len,src,copy_len);
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

		return (sub-dst);
	}

	inline int strpos( wchar_t const* const dst, wchar_t const* const check ) {
		wchar_t const* sub = wawo::strstr(dst,check);

		if(sub==NULL) {
			return -1;
		}

		return (sub-dst);
	}

	inline void strtolower( char* const buffer, u32_t const& size, char const* const src ) {
		WAWO_ASSERT(size>=wawo::strlen(src) );
		u32_t c = wawo::strlen(src);
		u32_t i = 0;
		while ( (i<c) && (i<size) )  {
			*(buffer + i) = tolower(*(src + i));
			++i;
		}
	}

	inline void strtoupper(char* const buffer, u32_t const& size, char const* const src) {
		WAWO_ASSERT(size >= wawo::strlen(src));
		u32_t c = wawo::strlen(src);
		u32_t i = 0;
		while ((i<c) && (i<size)) {
			*(buffer + i) = toupper(*(src + i));
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
	class len_cstr {
		//supported operation
		// 1, new & new with a init len
		// 2, init assign
		// 3, copy assign
		// 4, operator + for concat,
		// 5, len of string
		// 6, cstr address of string
		// 7, for any other requirement , please use std::string|std::wstring,,,
	public:
		typedef _char_t char_t;
		typedef _char_t* char_t_ptr;
	private:
		typedef len_cstr<char_t> _MyT;

		char_t* cstr;
		u32_t len;
	public:
		len_cstr():
			cstr( (char_t*)("")),
			len(0)
		{
		}

		len_cstr( char_t const* const str):
			cstr( (char_t*)("")),
			len(0)
		{
			WAWO_ASSERT( str != NULL );
			len = wawo::strlen(str) ;// str + '\0'

			if( len>0 ) {
				WAWO_ASSERT( cstr != str );
				cstr = (char_t*) malloc( sizeof(char_t)*(len+1) );
				WAWO_MALLOC_CHECK(cstr);
				wawo::strcpy(cstr,str);
			}
		}

		explicit len_cstr( char_t const* const str, u32_t const& slen ) :
			cstr( (char_t*)("")),
			len(0)
		{
			WAWO_ASSERT( wawo::strlen(str) >= slen );
			if( slen > 0 ) {
				len = slen ;
				cstr = (char_t*) malloc( sizeof(char)*(slen+1) ); // str + '\0'
				WAWO_MALLOC_CHECK(cstr);
				wawo::strncpy(cstr,str,slen);
			}
		}

		~len_cstr() {
			if(len>0){
				free(cstr);
			}
			cstr = NULL;
			len=0;
		}

		inline u32_t Len() const {
			return len;
		}
		inline char_t* CStr() const {
			return cstr;
		}

		len_cstr Substr( u32_t const& begin, u32_t const& length ) const {
			WAWO_ASSERT( ( begin + length) <= len );
			return len_cstr( cstr+begin, length );
		}

		void Swap( len_cstr& other ) {
			wawo::swap( cstr,other.cstr );
			wawo::swap( len, other.len );
		}

		len_cstr(len_cstr const& other ) :
			cstr( (char_t*)("")),
			len(0)
		{
			if( other.len > 0 ) {
				cstr = (char_t*) malloc ( sizeof(char_t)*(other.len+1) );
				WAWO_MALLOC_CHECK(cstr);

				len = other.len;
				//contain '\0'
				memcpy(cstr,other.cstr,(len+1)*sizeof(char_t) );
			}
		}

		len_cstr& operator = (len_cstr const& other) {
			len_cstr(other).Swap( *this );
			return *this;
		}

		//str must be null terminated, or behaviour undefined
		len_cstr& operator = ( char const* const str ) {
			len_cstr(str).Swap(*this);
			return *this;
		}

		inline bool operator == (len_cstr const& other) const {
			return (len == other.len) && (wawo::strncmp( cstr,other.cstr,len ) == 0) ;
		}
		inline bool operator != (len_cstr const& other) const {
			return (len != other.len) || (wawo::strncmp( cstr,other.cstr,len ) != 0) ;
		}

		inline bool operator < (len_cstr const& other) const {
			return wawo::strcmp(cstr, other.cstr) < 0;
		}

		inline bool operator > (len_cstr const& other) const {
			return wawo::strcmp(cstr, other.cstr) > 0;
		}

		_MyT operator + (len_cstr const& other) const {
			u32_t buffer_len = Len() + other.Len() + 1;
			char_t* _buffer = (char_t*) ::calloc( sizeof(char_t), buffer_len );

			if( Len() ) {
				wawo::strncpy( _buffer, CStr(), Len() );
			}

			if( other.Len() ) {
				wawo::strncat( _buffer, other.CStr(), other.Len() );
			}

			_MyT lcstr(_buffer, Len() + other.Len() );
			::free(_buffer);

			return lcstr;
		}

		_MyT operator + ( char const* const str ) const {
			return *this + len_cstr( str );
		}

		_MyT& operator += ( len_cstr const& other ) {
			_MyT tmp = (*this) + other;
			tmp.Swap(*this);
			return *this;
		}

		_MyT& operator += (char const* const str) {
			_MyT tmp = (*this) + str;
			tmp.Swap(*this);
			return *this;
		}
	};

	typedef len_cstr<char>		Len_CStr;
	typedef len_cstr<wchar_t>	Len_WCStr;

	inline Len_CStr	to_string(i64_t const& int64) {
		char tmp[32] = {0};
		snprintf(tmp, sizeof(tmp)/sizeof(tmp[0]), "%lld", int64 );
		return Len_CStr(tmp,wawo::strlen(tmp));
	}
	inline Len_CStr	to_string(u64_t const& uint64) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%llu", uint64);
		return Len_CStr(tmp, wawo::strlen(tmp));
	}
	inline Len_CStr	to_string(i32_t const& int32) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%d", int32);
		return Len_CStr(tmp, wawo::strlen(tmp));
	}
	inline Len_CStr	to_string(u32_t const& uint32) {
		char tmp[32] = { 0 };
		snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "%ud", uint32);
		return Len_CStr(tmp, wawo::strlen(tmp));
	}

	inline void split(wawo::Len_CStr const& string, wawo::Len_CStr const& delimiter, std::vector<wawo::Len_CStr>& result ) {

		WAWO_ASSERT( result.size() == 0 );
		char const* check_cstr = string.CStr();
		u32_t check_len = string.Len();
		u32_t next_check_idx = 0;

		char const* delimiter_cstr = delimiter.CStr();
		u32_t delimiter_len = delimiter.Len();

		int hit_pos ;

		while( (next_check_idx < check_len) && ( hit_pos = wawo::strpos( check_cstr + next_check_idx, delimiter_cstr )) != -1 )
		{
			wawo::Len_CStr tmp( (check_cstr + next_check_idx), hit_pos );
			next_check_idx += (hit_pos + delimiter_len);

			if( tmp.Len() > 0 ) {
				result.push_back( tmp );
			}
		}

		if( (check_len - next_check_idx) > 0 ) {
			wawo::Len_CStr left_string( (check_cstr + next_check_idx), check_len-next_check_idx );
			result.push_back( left_string );
		}
	}

	inline void split( std::string const& string, std::string const& delimiter_string, std::vector<std::string>& result ) {
		std::vector<Len_CStr> len_cstr_result;
		split( Len_CStr( string.c_str(), string.length()), Len_CStr(delimiter_string.c_str(), delimiter_string.length()), len_cstr_result );

		std::for_each( len_cstr_result.begin(), len_cstr_result.end(), [&result]( Len_CStr const& lencstr ) {
			result.push_back( std::string( lencstr.CStr(), lencstr.Len()) );
		});
	}

	inline void join( std::vector<wawo::Len_CStr> const& strings, wawo::Len_CStr const& delimiter, wawo::Len_CStr& result ) {
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
		std::vector<Len_CStr> len_cstr_vector;

		std::for_each( strings.begin(), strings.end(), [&len_cstr_vector]( std::string const& sstr ) {
			len_cstr_vector.push_back( Len_CStr( sstr.c_str(), sstr.length() ) );
		});

		Len_CStr len_cstr;
		join( len_cstr_vector, Len_CStr( delimiter.c_str(), delimiter.length()), len_cstr );
		result = std::string( len_cstr.CStr(), len_cstr.Len() );
	}
}
#endif
