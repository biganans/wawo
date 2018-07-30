#ifndef WAWO_MEMORY_HPP_
#define WAWO_MEMORY_HPP_

#include <wawo/exception.hpp>

namespace wawo {

	struct memory_alloc_failed:
		public wawo::exception
	{ 
		memory_alloc_failed(wawo::size_t const& size, const char* const file, const int line, const char* const func ) :
			exception(wawo::get_last_errno(), "", file,line,func, true)
		{
			::memset(message,0, WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT);
			int rt = snprintf(message, WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT, "memory alloc failed, alloc size: %zu", size);
			WAWO_ASSERT((rt>0) && (rt<WAWO_EXCEPTION_MESSAGE_LENGTH_LIMIT));
			(void)rt;
		}
	};
}

#define WAWO_ALLOC_CHECK(pointer, hintsize) \
	do { \
		if(pointer==NULL) { \
			throw wawo::memory_alloc_failed(hintsize, __FILE__, __LINE__,__FUNCTION__); \
		} \
	} while(0)

#define WAWO_DELETE(PTR) {do { if( PTR != 0 ) {delete PTR; PTR=NULL;} } while(false);}


#endif