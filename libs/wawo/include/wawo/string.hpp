#ifndef _WAWO_STRING_HPP_
#define _WAWO_STRING_HPP_

#include <cstring>
#include <cwchar>

#include <wawo/config/platform.h>

#define _WAWO_IS_CHAR_T(_T) (sizeof(_T)==sizeof(char))
#define _WAWO_IS_WCHAR_T(_T) (sizeof(_T)==sizeof(wchar_t))


//extern part
namespace wawo {

	//typedef uint32_t size_t;

	/* Please read these lines first if u have any question on these string apis
	 * why We have these APIs ?
	 *
	 * 1, std::strcpy is not safe
	 * 2, strcpy_s and strlcpy's parameter sequence is different
	 * 3, we don't have a wcsncpy_s on posix
	 * 4, there would be no obvious performance difference if we imply it by memxxx APIs
	 * 5, if I'm wrong, please correct me, thx.
	 */

	inline uint32_t strlen( char const* const str ) {
		return std::strlen(str);
	}

	inline uint32_t strlen(wchar_t const* const wstr ) {
		return std::wcslen(wstr);
	}

	inline int strcmp(char const* const lcptr, char const* const rcptr ) {
		return std::strcmp(lcptr,rcptr);
	}
	inline int strncmp(char const* const lcptr, char const* const rcptr, uint32_t const& len ) {
		return std::strncmp(lcptr,rcptr,len);
	}

	inline int strcmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr ) {
		return std::wcscmp(lwcptr,rwcptr);
	}

	inline int strncmp(wchar_t const* const lwcptr, wchar_t const* const rwcptr, uint32_t const& len) {
		return std::wcsncmp(lwcptr,rwcptr,len);
	}

	//src must be null-terminated
	//The behavior is undefined if the dest array is not large enough. The behavior is undefined if the strings overlap
	inline char* strcpy(char* const dst, char const* const src) {
		uint32_t src_len = wawo::strlen(src);
		std::memcpy(dst,src,src_len);
		*(dst+src_len) = '\0';
		return dst;
	}
	inline wchar_t* strcpy(wchar_t* const dst, wchar_t const* const src) {
		uint32_t src_len = wawo::strlen(src);
		std::memcpy(dst,src,src_len*sizeof(wchar_t) );
		*(dst+src_len) = L'\0';
		return dst;
	}

	inline char* strncpy(char* const dst, char const* const src, uint32_t const& len) {
		uint32_t src_len = wawo::strlen(src);
		uint32_t copy_len = (src_len>len)?len:src_len;
		std::memcpy(dst,src,copy_len*sizeof(char));
		*(dst+len) = '\0';
		return dst;
	}
	inline wchar_t* strncpy(wchar_t* const dst, wchar_t const* const src, uint32_t const& len) {
		uint32_t src_len = wawo::strlen(src);
		uint32_t copy_len = (src_len>len)?len:src_len;
		std::memcpy(dst,src,copy_len*sizeof(wchar_t));
		*(dst+len) = L'\0';
		return dst;
	}

	//src must be null-terminated, or the behaviour is undefined
	//dst must have enough space, or the behaviour is undefined if the strings overlap
	//dst will be null-terminated after strcat&strncat
	inline char* strcat(char* const dst, char const* const src) {
		uint32_t dst_len = wawo::strlen(dst);
		uint32_t src_len = wawo::strlen(src);

		wawo::strncpy(dst+dst_len,src,src_len);
		return dst;
	}
	inline char* strncat(char* const dst, char const* const src, uint32_t const& len) {
		uint32_t dst_len = wawo::strlen(dst);
		uint32_t src_len = wawo::strlen(src);
		uint32_t copy_len = (src_len>len) ? len : src_len;

		wawo::strncpy(dst+dst_len,src,copy_len);
		return dst;
	}
	inline wchar_t* strncat(wchar_t* const dst, wchar_t const* const src, uint32_t const& len) {
		uint32_t dst_len = wawo::strlen(dst);
		uint32_t src_len = wawo::strlen(src);
		uint32_t copy_len = (src_len>len) ? len : src_len;

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

}

namespace wawo {

	//supported operation
	// 1, new & new with a init len
	// 2, init assign
	// 3, copy assign
	// 4, operator + for contact,
	// 5, len of string
	// 6, cstr address of string

	// 7,for any other requirement , please use std::string|std::wstring,,,


	template <typename char_t>
	class len_cstr {
		typedef char_t _MyCharT;
		typedef len_cstr<char_t> _MyT;

	private:

		// basic type
		_MyCharT* cstr;
		// len of string
		uint32_t len;

	public:
		explicit len_cstr():
			cstr(NULL),
			len(0)
		{
		}

		explicit len_cstr(uint32_t const& slen ) :
			cstr(NULL),
			len(slen+1)
		{
			cstr = (_MyCharT*) malloc(sizeof(_MyCharT)*len);
			WAWO_MALLOC_CHECK(cstr);
		}

		explicit len_cstr( _MyCharT const* const str):
			cstr(NULL),
			len(0)
		{
			WAWO_ASSERT( str != NULL );

			len = wawo::strlen(str) + 1 ;// str + '\0'
			cstr = (_MyCharT*) malloc( sizeof(_MyCharT)*len );
			WAWO_MALLOC_CHECK(cstr);

			wawo::strcpy(cstr,str);
		}

		explicit len_cstr( _MyCharT const* const str, uint32_t const& slen ) :
			cstr(NULL),
			len(0)
		{
			len = slen + 1;
			cstr = (_MyCharT*) malloc( sizeof(char)*len ); // str + '\0'
			WAWO_MALLOC_CHECK(cstr);

			wawo::strncpy(cstr,str,slen);
		}

		~len_cstr() {
			free(cstr);
			cstr=NULL;
			len=0;
		}

		inline uint32_t Len() {
			return len-1;
		}
		inline uint32_t Len() const {
			return len-1;
		}
		inline _MyCharT* const CStr() {
			return cstr;
		}
		inline _MyCharT const* const CStr() const {
			return cstr;
		}

		len_cstr(len_cstr const& other ) :
			cstr(NULL),
			len(0)
		{
			WAWO_ASSERT(cstr == NULL);

			cstr = (_MyCharT*) malloc ( sizeof(_MyCharT)*other.len );
			WAWO_MALLOC_CHECK(cstr);

			len = other.len;
			memcpy(cstr,other.cstr,(len)*sizeof(_MyCharT) );
		}

		len_cstr& operator = (len_cstr const& other) {

			if( cstr != NULL ) {
				free(cstr);
				len =0;
			}

			WAWO_ASSERT( cstr == NULL );
			WAWO_ASSERT( len == 0 );

			cstr = (_MyCharT*) malloc (sizeof(_MyCharT)*other.len);
			WAWO_MALLOC_CHECK(cstr);

			len = other.len;
			memcpy(cstr,other.cstr,(len)*sizeof(_MyCharT));
			return *this;
		}

		bool operator == (len_cstr const& other) {
			return (len == other.len) && (wawo::strncmp( cstr,other.cstr,len ) == 0) ;
		}
		bool operator == (len_cstr const& other) const {
			return (len == other.len) && (wawo::strncmp( cstr,other.cstr,len ) == 0) ;
		}
		bool operator != (len_cstr const& other) {
			return (len != other.len) || (wawo::strncmp( cstr,other.cstr,len ) != 0) ;
		}
		bool operator != (len_cstr const& other) const {
			return (len != other.len) || (wawo::strncmp( cstr,other.cstr,len ) != 0) ;
		}

		_MyT operator + (len_cstr const& other) {
			uint32_t buffer_len = Len() + other.Len() + 1;
			_MyCharT* _buffer = (_MyCharT*) malloc( sizeof(_MyCharT)*buffer_len );

			wawo::strncpy( _buffer, CStr(), Len() );
			wawo::strncat( _buffer, other.CStr(), other.Len() );

			_MyT lcstr(_buffer,buffer_len-1);
			WAWO_DELETE(_buffer);

			return lcstr;
		}
		_MyT operator + (len_cstr const& other) const {
			uint32_t buffer_len = Len() + other.Len() + 1;
			_MyCharT* _buffer = (_MyCharT*) malloc( sizeof(_MyCharT)*buffer_len );

			wawo::strncpy( _buffer, CStr(), Len() );
			wawo::strncat( _buffer, other.CStr(), other.Len() );

			_MyT lcstr(_buffer,buffer_len-1);
			WAWO_DELETE(_buffer);

			return lcstr;
		}
	};


	typedef len_cstr<char>		Len_CStr;
	typedef len_cstr<wchar_t>	Len_WCStr;
}
#endif
