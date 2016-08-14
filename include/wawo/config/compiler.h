#ifndef _WAWO_CONFIG_COMPILER_H_
#define _WAWO_CONFIG_COMPILER_H_

/*
MSVC++ 14.0 _MSC_VER == 1900 (Visual Studio 2015)
MSVC++ 12.0 _MSC_VER == 1800 (Visual Studio 2013)
MSVC++ 11.0 _MSC_VER == 1700 (Visual Studio 2012)
MSVC++ 10.0 _MSC_VER == 1600 (Visual Studio 2010)
MSVC++ 9.0  _MSC_VER == 1500 (Visual Studio 2008)
MSVC++ 8.0  _MSC_VER == 1400 (Visual Studio 2005)
MSVC++ 7.1  _MSC_VER == 1310 (Visual Studio 2003)
MSVC++ 7.0  _MSC_VER == 1300
MSVC++ 6.0  _MSC_VER == 1200
MSVC++ 5.0  _MSC_VER == 1100
*/

//for compiler 
#if __cplusplus<201103L
	#define override
#endif

#define _WAWO_COMPILER_MSVC			1
#define _WAWO_COMPILER_GNUGCC		2

#if defined(_MSC_VER)
	#define WAWO_COMPILER_STR msvc
	#define WAWO_COMPILER	_WAWO_COMPILER_MSVC
#elif defined(__GNUC__)
	#define WAWO_COMPILER_STR gnugcc
	#define WAWO_COMPILER	_WAWO_COMPILER_GNUGCC
#else
	#error
#endif

#define WAWO_ISMSVC		(WAWO_COMPILER==_WAWO_COMPILER_MSVC)
#define WAWO_ISGNUGCC	(WAWO_COMPILER==_WAWO_COMPILER_GNUGCC)

#define _WAWO_ADDRESS_MODEL_X32	32
#define _WAWO_ADDRESS_MODEL_X64	64

#if _WIN32 || _WIN64
	#if _WIN64
		#define WAWO_ADDRESS_MODEL _WAWO_ADDRESS_MODEL_X64
	#else
		#define WAWO_ADDRESS_MODEL _WAWO_ADDRESS_MODEL_X32
	#endif
#endif

#if __GNUC__
	#if __x86_64__ || __ppc64__
		#define WAWO_ADDRESS_MODEL _WAWO_ADDRESS_MODEL_X64
	#else
		#define WAWO_ADDRESS_MODEL _WAWO_ADDRESS_MODEL_X32
	#endif
#endif

#define WAWO_ADDRESSMODE_X32 (WAWO_ADDRESS_MODEL==_WAWO_ADDRESS_MODEL_X32)
#define WAWO_ADDRESSMODE_X64 (WAWO_ADDRESS_MODEL==_WAWO_ADDRESS_MODEL_X64)

#define WAWO_COMPILER_HEADER(path)				WAWO_COMPILER_HEADER1(path, WAWO_COMPILER_STR)
#define WAWO_COMPILER_HEADER1(path,compiler)	WAWO_COMPILER_HEADER2(path,compiler)
#define WAWO_COMPILER_HEADER2(path,compiler)	<path##_##compiler##_.h>

#include WAWO_COMPILER_HEADER(wawo/config/compiler/compiler)

namespace wawo {
	typedef signed char			i8_t;
	typedef short				i16_t;
	typedef int					i32_t;
	typedef long long			i64_t;
	typedef unsigned char		u8_t;
	typedef unsigned short		u16_t;
	typedef unsigned int		u32_t;
	typedef unsigned long long	u64_t;
	typedef u8_t				byte_t;

#if WAWO_ADDRESSMODE_X64
	typedef u64_t				size_t;
#elif WAWO_ADDRESSMODE_X32
	typedef u32_t				size_t;
#else
	#error
#endif

}
#endif