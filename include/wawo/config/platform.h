#ifndef _WAWO_CONFIG_PLATFORM_H_
#define _WAWO_CONFIG_PLATFORM_H_

#define _WAWO_PLATFORM_WIN	1
#define _WAWO_PLATFORM_GNU	2

#if _WIN32 || _WIN64
	#define WAWO_PLATFORM_WIN		1
	#define WAWO_PLATFORM			_WAWO_PLATFORM_WIN
	#define WAWO_PLATFORM_STR		win
#elif __GNUC__
	#define WAWO_PLATFORM_GNU		1
	#define WAWO_PLATFORM			_WAWO_PLATFORM_GNU
	#define WAWO_PLATFORM_STR		gnu
#else
	#error
#endif

#define WAWO_ISWIN	(WAWO_PLATFORM==_WAWO_PLATFORM_WIN)
#define WAWO_ISGNU	(WAWO_PLATFORM==_WAWO_PLATFORM_GNU)


#define WAWO_PLATFORM_HEADER(path)				WAWO_PLATFORM_HEADER1(path, WAWO_PLATFORM_STR)
#define WAWO_PLATFORM_HEADER1(path,platform)	WAWO_PLATFORM_HEADER2(path,platform)
#define WAWO_PLATFORM_HEADER2(path,platform)	<path##_##platform##_.h>

#include WAWO_PLATFORM_HEADER(wawo/config/platform/platform)
#endif