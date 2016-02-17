#ifndef _WAWO_PROTOTYPE_EXCEPTION_HPP_
#define _WAWO_PROTOTYPE_EXCEPTION_HPP_


#include <cstring>
#include <string>

#include <wawo/config/platform.h>
#include <wawo/basic.hpp>
#include <wawo/string.hpp>

#ifdef WAWO_PLATFORM_POSIX
	#include <execinfo.h>
#endif

#define WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT 512
#define WAWO_EXCEPTION_FILE_LENGTH_LIMIT 256
#define WAWO_EXCEPTION_FUNC_LENGTH_LIMIT 256
#define WAWO_EXCEPTION_CALLSTACK_LENGTH_LIMIT 4096

namespace wawo {

#ifdef WAWO_PLATFORM_POSIX
	extern void trace( char stack_buffer[], int const& s );
#endif

	struct Exception {
		int code;
		Len_CStr info;

		Len_CStr file;
		int line;

		Len_CStr func;
		Len_CStr callstack ;

		explicit Exception( int const& e_code, char const* const e_info, char const* const e_file, char const* const e_func , int const& e_line )
			:
			code(e_code),
			info(e_info),
			file(e_file),
			line(e_line),
			func(e_func)
		{

#ifdef WAWO_PLATFORM_POSIX
			char buffer[4096];
			trace( buffer, 4096 ) ;

			//printf("exception buffer: %s\n", buffer );
			callstack = Len_CStr(buffer);
			//printf("exception callstack: %s\n", callstack.cstr );
#endif
		}

		~Exception() {}

		Exception(Exception const& other ) {
			code = other.code;
			info = other.info;
			file = other.file;
			line = other.line;
			func = other.func;
			callstack = other.callstack;
		}

		char const* GetInfo() {
			return info.CStr() ;
		}

		int const& GetCode() {
			return code;
		}

		char* const GetFile() {
			return file.CStr();
		}

		char* const GetFunc() {
			return func.CStr();
		}

		int const& GetLine() {
			return line;
		}

		char* const GetStackTrace() {
			return callstack.CStr();
		}
	};

}


#define WAWO_THROW_EXCEPTION2(code,msg) throw wawo::Exception(code,msg,__FILE__, __FUNCTION__,__LINE__ );
#define WAWO_THROW_EXCEPTION(msg) WAWO_THROW_EXCEPTION2(0,msg)

//#define WAWO_THROW_EXCEPTION2(code,msg) throw wawo::Exception(code,wawo::len_cstr(msg), __FILE__, __FUNCTION__,__LINE__, "" );
#define WAWO_THROW(x) throw x;


#ifndef _DEBUG
	#define THROW_EXCEPTION_FOR_ASSERT
#endif


#define WAWO_CONDITION_CHECK(statement) \
	do { \
		if(!(statement)) { \
			WAWO_THROW_EXCEPTION( "statement ("#statement ") check failed exception" ); \
		} \
	} while(0);

#define WAWO_NULL_POINT_CHECK(x) WAWO_CONDITION_CHECK(x!=NULL)

#ifdef THROW_EXCEPTION_FOR_ASSERT
	#ifdef WAWO_ASSERT
		#undef WAWO_ASSERT
	#endif
	#define WAWO_ASSERT(x) WAWO_CONDITION_CHECK(x)
#endif

#endif
