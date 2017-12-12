#ifndef WAWO_MEMORY_HPP_
#define WAWO_MEMORY_HPP_

#include <wawo/exception.hpp>

namespace wawo {

	struct memory_alloc_failed:
		public wawo::exception
	{ 
		memory_alloc_failed(int const& size, const char* const file, const int line, const char* const func ) :
			exception(wawo::get_last_errno(), "", file,line,func, true)
		{
			char tmp[128] = { 0 };
			snprintf(tmp, 128, "memory alloc failed, alloc size: %d", size);
			int _len = ::strlen(tmp);
			::memcpy(message, tmp, _len);
			*(message + _len) = '\0';
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