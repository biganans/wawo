#ifndef _WAWO_MACROS_H_
#define _WAWO_MACROS_H_

#include <cassert>


#ifndef NULL
	#define NULL 0
#endif

#ifndef _DEBUG
	#ifdef DEBUG
		#define _DEBUG
	#endif
#endif

#ifndef _DEBUG
	#define CHECK_FOR_RELEASE 1
#endif

#define WAWO_MIN2(A,B)		((A)>(B)?(B):(A))
#define WAWO_MIN3(A,B,C)	(WAWO_MIN((A),(B)),(C))
#define WAWO_MAX2(A,B)		((A)>(B)?(A):(B))
#define WAWO_MAX3(A,B,C)	(WAWO_MAX((A),(B)),(C))

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

#define WAWO_ASSERT(x) assert(x) // @todo ,let's polish this latter ,,,,,,,
#define WAWO_MALLOC_CHECK(point) \
	do { \
		if(NULL==point) { \
			std::exit( wawo::E_MALLOC_FAILED ); \
		} \
	} while(0);

#define WAWO_RETURN_V_IF_MATCH(v,condition) \
	do { \
		if( (condition) ) { return (v);} \
	} while(0);

#define WAWO_RETURN_V_IF_NOT_MATCH(v,condition) WAWO_RETURN_V_IF_MATCH(v, !(condition))

#define WAWO_RETURN_IF_MATCH(condition) \
	do { \
		if( (condition) ) { return ;} \
	} while(0);

#define WAWO_EXIT(exit_code,message) {WAWO_LOG_INFO("WAWO", "exit message: %s", message );exit(exit_code);}

#endif
