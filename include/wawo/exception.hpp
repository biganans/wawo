#ifndef _WAWO_EXCEPTION_HPP_
#define _WAWO_EXCEPTION_HPP_

#include <wawo/config/platform.h>
#include <wawo/basic.hpp>

#ifdef WAWO_PLATFORM_GNU
	#include <execinfo.h>
#endif

#define WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT 1024
#define WAWO_EXCEPTION_FILE_LENGTH_LIMIT 512
#define WAWO_EXCEPTION_FUNC_LENGTH_LIMIT 512

namespace wawo {

	void stack_trace( char stack_buffer[], u32_t const& s );

	struct exception {
		int code;
		char message[WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT];
		char file[WAWO_EXCEPTION_FILE_LENGTH_LIMIT];
		int line;
		char func[WAWO_EXCEPTION_FUNC_LENGTH_LIMIT];

		char* callstack;

		explicit exception(int const& code, char const* const sz_message_, char const* const sz_file_ , int const& line_ , char const* const sz_func_ ,  bool const& no_stack_info = false );
		virtual ~exception();

		exception(exception const& other);
		exception& operator= (exception const& other);
	};
}

#define WAWO_THROW2(code,msg) throw wawo::exception(code,msg,__FILE__, __LINE__,__FUNCTION__ );
#define WAWO_THROW(msg) WAWO_THROW2(wawo::get_last_errno(),msg)

#define WAWO_CONDITION_CHECK(statement) \
	do { \
		if(!(statement)) { \
			WAWO_THROW( "statement ("#statement ") check failed exception" ); \
		} \
	} while(0);
#endif