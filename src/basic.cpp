
#include <chrono>
#include <wawo/macros.h>
#include <wawo/basic.hpp>

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
		u64_t a = rand() & 0xFFFF;
		u64_t b = rand() & 0xFFFF;
		u64_t c = rand() & 0xFFFF;
		u64_t d = rand() & 0xFFFF;
		return (a << 48 | b << 32 | c << 16 | d);
	}
	u32_t random_u32(bool const& upseed ) {
		if (upseed) srand();
		u32_t a = rand() & 0xFFFF;
		u32_t b = rand() & 0xFFFF;
		return (a << 16) | b ;
	}
	u16_t random_u16(bool const& upseed ) {
		if (upseed) srand();
		return std::rand();
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

#ifdef WAWO_PLATFORM_WIN
	const int errorMsgLen = 1024;
	void appError(const char* error, const char* file , int line , const char* function, ...)
	{
		char errorMsg[errorMsgLen];
		va_list argp;
		va_start(argp, function);
		vsprintf_s(errorMsg, error, argp);
		va_end(argp);

		char assertBuf[errorMsgLen + 512];
		snprintf(assertBuf, errorMsgLen + 512, "Error : %s\nFile : %s\nLine : %d\nFunction : %s",
				errorMsg, file, line, function);

		int ret = 2;
#ifdef WIN32
		WCHAR wcharAssertBuf[(sizeof(assertBuf)  + 1) * 2];
		MultiByteToWideChar(CP_ACP,0,assertBuf,strlen(assertBuf) + 1,wcharAssertBuf,sizeof(wcharAssertBuf)/sizeof(WCHAR));
		ret = MessageBox(GetActiveWindow(), wcharAssertBuf, L"AssertMsg", MB_OKCANCEL|MB_ICONERROR);
#endif
		if (ret > 1) __asm int 3;

		//memset(0, 0, 1); //force crash
		return;
	}
	#endif
}
