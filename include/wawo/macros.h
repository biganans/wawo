#ifndef _WAWO_MACROS_H_
#define _WAWO_MACROS_H_

#include <cassert>

#include <wawo/config/compiler.h>
#include <wawo/config/platform.h>

namespace wawo {
	extern void assert_failed(const char* error, const char* file, int line, const char* function, ... );
}

#define WAWO_USE_WAWO_ASSERT_FOR_RELEASE
#define WAWO_MALLOC_H_RESERVE (128)

#ifndef NULL
	#define NULL 0
#endif

#ifdef DEBUG
	#ifndef _DEBUG
	#define _DEBUG
	#endif
#endif

#define WAWO_MIN2(A,B)		((A)>(B)?(B):(A))
#define WAWO_MIN3(A,B,C)	WAWO_MIN2((WAWO_MIN2((A),(B))),(C))
#define WAWO_MAX2(A,B)		((A)>(B)?(A):(B))
#define WAWO_MAX3(A,B,C)	WAWO_MAX2((WAWO_MAX2((A),(B))),(C))

#define WAWO_MIN(A,B)		WAWO_MIN2((A),(B))
#define WAWO_MAX(A,B)		WAWO_MAX2((A),(B))

#define WAWO_ABS2(A,B)		(((A)>(B))?((A)-(B)):((B)-(A)))
#define WAWO_ABS(A)			(WAWO_ABS2(A,0))
#define WAWO_NEGATIVE(A)	((A<0)?(A):(-(A)))

#define WAWO_STRINGIFY(str)	#str

#ifdef _DEBUG
#if WAWO_ISGNU
#define WAWO_ASSERT(x,...) assert(x)
#else
#define WAWO_ASSERT(x, ...) ((void) ((x) ? (void)0 : wawo::assert_failed (#x, __FILE__, __LINE__ , __FUNCTION__, "" __VA_ARGS__)))
#endif
#else
#ifndef WAWO_USE_WAWO_ASSERT_FOR_RELEASE
#define WAWO_ASSERT(x, ...)
#else
#define WAWO_ASSERT(x, ...) ( ((x) ? (void)0 : wawo::assert_failed (#x, __FILE__, __LINE__ , __FUNCTION__, "" __VA_ARGS__)))
#endif
#endif

#ifdef _WW_NO_CXX11_DELETED_FUNC
#define WAWO_DECLARE_NONCOPYABLE(__classname__) \
		private: \
		__classname__(__classname__ const&) ; \
		__classname__& operator=(__classname__ const&) ; \
		private:
#else
#define WAWO_DECLARE_NONCOPYABLE(__classname__) \
	protected: \
	__classname__(__classname__ const&) = delete; \
	__classname__& operator=(__classname__ const&) = delete; \
	private:
#endif

#define WAWO_RETURN_V_IF_MATCH(v,condition) \
	do { \
		if( (condition) ) { return (v);} \
	} while(0)

#define WAWO_RETURN_V_IF_NOT_MATCH(v,condition) WAWO_RETURN_V_IF_MATCH(v, !(condition))

#define WAWO_RETURN_IF_MATCH(condition) \
	do { \
		if( (condition) ) { return ;} \
	} while(0)
#endif