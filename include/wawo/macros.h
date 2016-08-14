#ifndef _WAWO_MACROS_H_
#define _WAWO_MACROS_H_

#include <cassert>
#include <wawo/config/platform.h>

#define WAWO_MALLOC_H_RESERVE (128)

#ifndef NULL
	#define NULL 0
#endif

#ifndef _DEBUG
	#ifdef DEBUG
		#define _DEBUG
	#endif
#endif


#define WAWO_MIN2(A,B)		((A)>(B)?(B):(A))
#define WAWO_MIN3(A,B,C)	WAWO_MIN2((WAWO_MIN2((A),(B))),(C))
#define WAWO_MAX2(A,B)		((A)>(B)?(A):(B))
#define WAWO_MAX3(A,B,C)	WAWO_MAX2((WAWO_MAX2((A),(B))),(C))

#define WAWO_MIN(A,B)		WAWO_MIN2((A),(B))
#define WAWO_MAX(A,B)		WAWO_MAX2((A),(B))

#define WAWO_ABS(A)			(((A)>0)?(A):(-(A)))
#define WAWO_NEGATIVE(A)	((A<0)?(A):(-(A)))

#define WAWO_STRINGIFY(str)	#str

/*
statement,
	x==3 && y==4 && z==5
	x
	y != 0
	func();
*/
#ifdef WAWO_ISGNU
	#define WAWO_ASSERT(x) assert(x) // @todo ,let's polish this latter ,,,,,,,
#else
	#define WAWO_ASSERT(to_check, ...) ((void) ((to_check) ? 0 : wawo::appError (#to_check, __FILE__, __LINE__ , __FUNCTION__, ##__VA_ARGS__)))
#endif

//#define WAWO_ASSERT_N(Dev, to_check, ...) ((void) ((to_check) ? 0 : wawo::appError ("["#Dev"]"#to_check, __FILE__, __LINE__ , __FUNCTION__, ##__VA_ARGS__)))

#define WAWO_MALLOC_CHECK(point) \
	do { \
		if(NULL==point) { \
			std::exit( wawo::E_MALLOC_FAILED ); \
		} \
	} while(0)

#define WAWO_RETURN_V_IF_MATCH(v,condition) \
	do { \
		if( (condition) ) { return (v);} \
	} while(0)

#define WAWO_RETURN_V_IF_NOT_MATCH(v,condition) WAWO_RETURN_V_IF_MATCH(v, !(condition))

#define WAWO_RETURN_IF_MATCH(condition) \
	do { \
		if( (condition) ) { return ;} \
	} while(0)


#define WAWO_EXIT(exit_code,message) {WAWO_INFO("[WAWO]exit message: %s", message );exit(exit_code);}

#endif
