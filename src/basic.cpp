
#include <cstdlib>
#include <stdarg.h>
#include <chrono>

#include <wawo/basic.hpp>
#include <wawo/string.hpp>
#include <wawo/exception.hpp>

#ifdef WAWO_PLATFORM_GNU
	#include <pthread.h>
#endif

namespace wawo {

	u32_t get_stack_size() {
	#ifdef WAWO_PLATFORM_GNU
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		::size_t s;
		pthread_attr_getstacksize( &attr, &s );
		int rt = pthread_attr_destroy(&attr);
		WAWO_ASSERT( rt == 0);
		(void)&rt;
		return s;
	#else
		return 0 ; //not implemented
	#endif
	}
	void srand( unsigned int const& seed ) {
		if (seed != 0) {
			std::srand(seed);
			return;
		}
		std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> tp1 = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
		std::srand((unsigned int)tp1.time_since_epoch().count());
	}
	u64_t random_u64(bool const& upseed ) {
		if (upseed) srand();
		u64_t a = std::rand() & 0xFFFF;
		u64_t b = std::rand() & 0xFFFF;
		u64_t c = std::rand() & 0xFFFF;
		u64_t d = std::rand() & 0xFFFF;
		return (a << 48 | b << 32 | c << 16 | d);
	}
	u32_t random_u32(bool const& upseed ) {
		if (upseed) srand();
		u32_t a = std::rand() & 0xFFFF;
		u32_t b = std::rand() & 0xFFFF;
		return (a << 16) | b ;
	}
	u16_t random_u16(bool const& upseed ) {
		if (upseed) srand();
		return std::rand()&0xFFFF;
	}
	u8_t random_u8(bool const& upseed) {
		if (upseed) srand();
		return (std::rand() & 0xFF);
	}

	//char destination[destinationLen];
	//char assertBuf[destinationLen + 512];
//#ifdef WIN32
//	WCHAR wcharAssertBuf[(sizeof(assertBuf)  + 1) * 2];
//#endif

	#define _ERR_MSG_MAX_LEN 1024
	void assert_failed(const char* check, const char* file , int line , const char* function, ... )
	{
		char _message[_ERR_MSG_MAX_LEN] = {0};
		va_list argp;
		va_start(argp, function);
		char* fmt = va_arg(argp, char*);
		vsnprintf(_message, _ERR_MSG_MAX_LEN,fmt, argp);
		va_end(argp);

		if (wawo::strlen(_message) == 0) {
			wawo::strcpy(_message, "no info attached");
		}

		char _assert_buf[_ERR_MSG_MAX_LEN + 1024] = {0};
		snprintf(_assert_buf, _ERR_MSG_MAX_LEN + 1024, "assert failed : %s, \nInfo: %s\nFile : %s\nLine : %d\nFunction : %s",
			check, _message, file, line, function);

#if defined(WAWO_ISWIN) && defined(WAWO_USE_MESSAGE_BOX)
		int ret = 2;
		WCHAR wcharAssertBuf[(sizeof(_assert_buf)  + 1) * 2];
		MultiByteToWideChar(CP_ACP,0, _assert_buf,strlen(_assert_buf) + 1,wcharAssertBuf,sizeof(wcharAssertBuf)/sizeof(WCHAR));
		ret = MessageBox(GetActiveWindow(), wcharAssertBuf, L"AssertMsg", MB_OKCANCEL|MB_ICONERROR);
		if (ret > 1) __asm int 3;
#else
		throw wawo::exception( wawo::get_last_errno(), _assert_buf, file, line, function );
#endif
	}
}