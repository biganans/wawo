#ifndef __WAWO___INIT___HPP_
#define __WAWO___INIT___HPP_

#include <wawo/mutex.hpp>
#include <wawo/thread.hpp>

#ifdef WAWO_PLATFORM_WIN
	#include <wawo/net/winsock_helper.hpp>
#endif
/////////////////////////////////////////
namespace wawo {

	class __init__:
		public wawo::singleton<__init__>
	{
		DECLARE_SINGLETON_FRIEND(__init__);
	protected:
		__init__() {
			static_assert(sizeof(wawo::i8_t) == 1, "assert sizeof(i8_t) failed");
			static_assert(sizeof(wawo::u8_t) == 1, "assert sizeof(u8_t) failed");
			static_assert(sizeof(wawo::i16_t) == 2, "assert sizeof(i16_t) failed");
			static_assert(sizeof(wawo::u16_t) == 2, "assert sizeof(u16_t) failed");
			static_assert(sizeof(wawo::i32_t) == 4, "assert sizeof(i32_t) failed");
			static_assert(sizeof(wawo::u32_t) == 4, "assert sizeof(u32_t) failed");
			static_assert(sizeof(wawo::i64_t) == 8, "assert sizeof(i64_t) failed");
			static_assert(sizeof(wawo::u64_t) == 8, "assert sizeof(u64_t) failed");

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
			//for mutex/lock deubg
			wawo::tls_create<wawo::MutexSet>() ;
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread_debugger::instance()->init();
#endif

#ifdef WAWO_PLATFORM_WIN
			wawo::net::winsock_helper::instance()->init();
#endif
		}
		~__init__() {
#ifdef _DEBUG_THREAD_CREATE_JOIN
			try {
				wawo::thread_debugger::instance()->deinit();
			}
			catch (...) {
			}
#endif

#ifdef WAWO_PLATFORM_WIN
			wawo::net::winsock_helper::instance()->deinit();
#endif
		}
	};
	static __init__& _ = *(__init__::instance());
}
////////////////////////////////////////////
#endif