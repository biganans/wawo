
#include <wawo/macros.h>
#include <wawo/basic.hpp>


#ifdef WAWO_PLATFORM_POSIX
	#include <pthread.h>
#endif


namespace wawo {

	uint32_t get_stack_size() {

	#ifdef WAWO_PLATFORM_POSIX

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		size_t s;
		pthread_attr_getstacksize( &attr, &s );

		int rt = pthread_attr_destroy(&attr);
		WAWO_ASSERT( rt == 0);

		return s;
	#else
		return 0 ; //not implemented
	#endif
	}

}
