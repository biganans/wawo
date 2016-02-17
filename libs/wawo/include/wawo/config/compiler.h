#ifndef _WAWO_CONFIG_COMPILER_H_
#define _WAWO_CONFIG_COMPILER_H_

//for compiler 
#if __cplusplus<201103L
	#define override
#endif

#define _WAWO_COMPILER_MSVC		1
#define _WAWO_COMPILER_GCC		2

#if defined( _MSC_VER )
	#define WAWO_COMPILER_STR msvc
	#define WAWO_COMPILER	_WAWO_COMPILER_MSVC
#elif defined(GCC) || defined(__GCC__) || defined(__GNUC__)
	#define WAWO_COMPILER_STR gcc
	#define WAWO_COMPILER	_WAWO_COMPILER_GCC
#else
	#error
#endif

#define IS_MSVC()	(WAWO_COMPILER==_WAWO_COMPILER_MSVC)
#define IS_GCC()	(WAWO_COMPILER==_WAWO_COMPILER_GCC)


#define WAWO_COMPILER_HEADER(path)				WAWO_COMPILER_HEADER1(path, WAWO_COMPILER_STR)
#define WAWO_COMPILER_HEADER1(path,compiler)	WAWO_COMPILER_HEADER2(path,compiler)
#define WAWO_COMPILER_HEADER2(path,compiler)	<path##_##compiler##_.h>

#include WAWO_COMPILER_HEADER(wawo/config/compiler/compiler)

namespace wawo {
	typedef basic_type::_INT8_					int8_t		;
	typedef basic_type::_INT16_					int16_t		;
	typedef basic_type::_INT32_					int32_t		;
	typedef	basic_type::_INT64_					int64_t		;

	typedef basic_type::_UINT8_					byte_t		;
	typedef basic_type::_UINT8_					uint8_t		;
	typedef basic_type::_UINT16_				uint16_t	;
	typedef basic_type::_UINT32_				uint32_t	;
	typedef basic_type::_UINT64_				uint64_t	;
}
#endif
