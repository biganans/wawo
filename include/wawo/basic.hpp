#ifndef _WAWO_BASIC_HPP_
#define _WAWO_BASIC_HPP_

#include <chrono>
#include <thread>

#include <wawo/config/platform.h>
#include <wawo/config/compiler.h>
#include <wawo/macros.h>
#include <wawo/constants.h>

namespace wawo {
	class NonCopyable {
	protected:
		NonCopyable() {}
		~NonCopyable() {}
	private:
		NonCopyable(NonCopyable const&);
		NonCopyable& operator=(NonCopyable const&);
	};
}

//inline part
namespace wawo {
	inline int GetLastErrno() {
#ifdef WAWO_PLATFORM_GNU
		return errno;
#elif defined(WAWO_PLATFORM_WIN)
		return ::GetLastError();
#else
		#error
#endif
	}

	//remark: WSAGetLastError() == GetLastError(), but there is no gurantee for future change.
	inline int SocketGetLastErrno() {
#ifdef WAWO_PLATFORM_GNU
		return errno;
#elif defined(WAWO_PLATFORM_WIN)
		return ::WSAGetLastError();
#else
		#error
#endif
	}

	inline bool IsBigEndian() {
		unsigned int x = 1;
		return 0 == *(unsigned char*)(&x);
	}

	inline bool IsLittleEndian() {
		return !IsBigEndian();
	}

	//non-atomic ,,, be carefully,,
	template <typename T>
	inline void swap(T& a, T& b) {
		WAWO_ASSERT(&a != &b);
		T tmp = a;
		a = b;
		b = tmp;
	}

	inline void sleep( wawo::u32_t const& milliseconds ) {
		std::chrono::milliseconds dur( milliseconds );
		std::this_thread::sleep_for( dur );
	}
	inline void usleep(wawo::u32_t const& microseconds ) {
		std::chrono::microseconds dur( microseconds );
		std::this_thread::sleep_for (dur) ;
	}
	inline void nsleep(wawo::u32_t const& nanoseconds ) {
		std::chrono::nanoseconds dur(nanoseconds);
		std::this_thread::sleep_for(dur);
	}
	inline void yield() {
		std::this_thread::yield();
	}
	inline void yield(u32_t const& k) {
		if( k<4 ) {
		} else if( k<32) {
			std::this_thread::yield();
		} else {
			wawo::nsleep(k);
		}
	}

	typedef wawo::u64_t Tid;

	inline Tid get_thread_id() {
		std::hash<std::thread::id> hasher;
		return static_cast<wawo::u64_t>(hasher(std::this_thread::get_id()));
	}
}

namespace wawo {
	extern u32_t get_stack_size() ;
	extern void appError(const char* error, const char* file , int line , const char* function, ...);
}

#include <atomic>
//atomic
namespace wawo {
	template <typename T>
	inline T atomic_increment( std::atomic<T>* atom) {
		return atom->fetch_add(1, std::memory_order_relaxed );
	}

	template <typename T>
	inline T atomic_decrement(std::atomic<T>* atom) {
		return atom->fetch_sub(1, std::memory_order_acq_rel);
	}

	template <typename T>
	inline T atomic_increment_if_not_equal(std::atomic<T>* atom, T const& val) {
		T tv = atom->load( std::memory_order_relaxed );
		WAWO_ASSERT( tv >= val );
		for(;;) {
			if( tv == val ) {
				return tv;
			}
			if( atom->compare_exchange_weak(tv,tv+1, std::memory_order_relaxed, std::memory_order_relaxed )) {
				return tv;
			}
		}
	}
}

namespace wawo {

		//union data contain 64 bit
		union UData64 {
		explicit UData64() :int64_v(0){}
		explicit UData64(u64_t const& u64_v) : uint64_v(u64_v){}
		explicit UData64(u32_t const& u32_v) : uint32_v(u32_v){}
		explicit UData64(u16_t const& u16_v) : uint16_v(u16_v){}
		explicit UData64(u8_t const& u8_v) : uint8_v(u8_v){}
		explicit UData64(i64_t const& i64_v) : int64_v(i64_v){}
		explicit UData64(i32_t const& i32_v) : int32_v(i32_v){}
		explicit UData64(i16_t const& i16_v) : int16_v(i16_v){}
		explicit UData64(i8_t const& i8_v) : int8_v(i8_v){}
		explicit UData64( void* const ptr) : ptr_v(ptr) {}

		u64_t uint64_v;
		u32_t uint32_v;
		u16_t uint16_v;
		u8_t uint8_v;

		i64_t int64_v;
		i32_t int32_v;
		i16_t int16_v;
		i8_t int8_v;
		void* ptr_v;
	};
}

namespace wawo {
	void		srand(unsigned int const& seed = 0);
	u64_t	random_u64(bool const& upseed = false);
	u32_t	random_u32(bool const& upseed = false);
	u16_t	random_u16(bool const& upseed = false);
	u8_t		random_u8(bool const& upseed = false);
}

#endif
