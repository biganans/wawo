#ifndef __WAWO___INIT___HPP_
#define __WAWO___INIT___HPP_

#include <wawo/thread/Mutex.hpp>
#include <wawo/thread/Thread.hpp>

/////////////////////////////////////////
namespace __wawo__ {

	class __init__:
		public wawo::Singleton<__init__>
	{
		DECLARE_SINGLETON_FRIEND(__init__);
	protected:
		__init__() {

			/*
			printf("sizeof(wawo::i8_t)=%d\n", sizeof(wawo::i8_t));
			printf("sizeof(wawo::i16_t)=%d\n", sizeof(wawo::i16_t));
			printf("sizeof(wawo::i32_t)=%d\n", sizeof(wawo::i32_t));
			printf("sizeof(wawo::i64_t)=%d\n", sizeof(wawo::i64_t));

			printf("sizeof(wawo::u8_t)=%d\n", sizeof(wawo::u8_t));
			printf("sizeof(wawo::u16_t)=%d\n", sizeof(wawo::u16_t));
			printf("sizeof(wawo::u32_t)=%d\n", sizeof(wawo::u32_t));
			printf("sizeof(wawo::u64_t)=%d\n", sizeof(wawo::u64_t));
			*/

			WAWO_ASSERT(sizeof(wawo::i8_t) == 1);
			WAWO_ASSERT(sizeof(wawo::u8_t) == 1);
			WAWO_ASSERT(sizeof(wawo::i16_t) == 2);
			WAWO_ASSERT(sizeof(wawo::u16_t) == 2);
			WAWO_ASSERT(sizeof(wawo::i32_t) == 4);
			WAWO_ASSERT(sizeof(wawo::u32_t) == 4);
			WAWO_ASSERT(sizeof(wawo::i64_t) == 8);
			WAWO_ASSERT(sizeof(wawo::u64_t) == 8);

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
			//for mutex/lock deubg
			WAWO_TLS_INIT(wawo::thread::MutexSet) ;
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread::ThreadDebugger::GetInstance()->Init();
#endif
		}
		~__init__() {
#ifdef _DEBUG_THREAD_CREATE_JOIN
			try {
				wawo::thread::ThreadDebugger::GetInstance()->Deinit();
			}
			catch (...) {
			}
#endif
		}
	};
	static __init__& _ = *(__init__::GetInstance());
}
////////////////////////////////////////////
#endif