#ifndef _WAWO_BASIC_HPP_
#define _WAWO_BASIC_HPP_

#include <utility>
#include <atomic>

#include <wawo/config/compiler.h>
#include <wawo/config/platform.h>
#include <wawo/macros.h>
#include <wawo/constants.h>

//inline part
namespace wawo {
	inline int get_last_errno() {
#ifdef WAWO_PLATFORM_GNU
		return WAWO_NEGATIVE(errno);
#elif defined(WAWO_PLATFORM_WIN)
		int ec = (::GetLastError());
		return WAWO_NEGATIVE(ec);
#else
		#error
#endif
	}

	inline void set_last_errno(int e) {
#ifdef WAWO_PLATFORM_GNU
		errno=e;
#elif defined(WAWO_PLATFORM_WIN)
		::SetLastError(e);
#else
	#error
#endif
	}

	//remark: WSAGetLastError() == GetLastError(), but there is no gurantee for future change.
	inline int socket_get_last_errno() {
#ifdef WAWO_PLATFORM_GNU
		return WAWO_NEGATIVE(errno);
#elif defined(WAWO_PLATFORM_WIN)
		int ec=(::WSAGetLastError());
		return WAWO_NEGATIVE(ec);
#else
		#error
#endif
	}

	inline bool is_big_endian() {
		unsigned int x = 1;
		return 0 == *(unsigned char*)(&x);
	}

	inline bool is_little_endian() {
		return !is_big_endian();
	}

	namespace deprecated {
		//@2018-01-23 deprecated, please use std::swap instead
		//non-atomic ,,, be carefully,,
		template <typename T>
		inline void swap(T& a, T& b) _WW_NOEXCEPT {
			WAWO_ASSERT(&a != &b);
			T tmp(std::move(a));
			a = std::move(b);
			b = std::move(tmp);
		}
	}

	template <class L, class R>
	struct is_same_class
	{
		static bool const value = false;
	};

	template <class L>
	struct is_same_class<L, L>
	{
		static bool const value = true;
	};
}

namespace wawo {
	u32_t get_stack_size() ;
	void assert_failed(const char* error, const char* file , int line , const char* function, ...);
}

namespace wawo {
	template <typename T>
	inline T atomic_increment( std::atomic<T>* atom,std::memory_order order = std::memory_order_acq_rel) _WW_NOEXCEPT {
		return atom->fetch_add(1, order);
	}

	template <typename T>
	inline T atomic_decrement(std::atomic<T>* atom, std::memory_order order = std::memory_order_acq_rel) _WW_NOEXCEPT {
		return atom->fetch_sub(1, order);
	}

	template <typename T>
	inline T atomic_increment_if_not_equal(std::atomic<T>* atom, T const& val,
		std::memory_order success = std::memory_order_acq_rel,
		std::memory_order failure = std::memory_order_acquire ) _WW_NOEXCEPT
	{
		T tv = atom->load(failure);
		WAWO_ASSERT( tv >= val );
		for(;;) {
			if( tv == val ) {
				return tv;
			}
			if( atom->compare_exchange_weak(tv,tv+1, success, failure)) {
				return tv;
			}
		}
	}

	template <bool b>
	inline bool is_true() {
		return true;
	}

	template <>
	inline bool is_true<false>() { return false; }

	template <typename T>
	struct less {
		inline bool operator()(T const& a, T const& b)
		{
			return a<b;
		}
	};

	template <typename T>
	struct greater {
		inline bool operator()(T const& a, T const& b)
		{
			return a>b;
		}
	};
}

namespace wawo {

		//union data contain 64 bit
		union udata64 {
		explicit udata64() :int64_v(0){}
		explicit udata64(u64_t const& u64_v) : uint64_v(u64_v){}
		explicit udata64(u32_t const& u32_v) : uint32_v(u32_v){}
		explicit udata64(u16_t const& u16_v) : uint16_v(u16_v){}
		explicit udata64(u8_t const& u8_v) : uint8_v(u8_v){}
		explicit udata64(i64_t const& i64_v) : int64_v(i64_v){}
		explicit udata64(i32_t const& i32_v) : int32_v(i32_v){}
		explicit udata64(i16_t const& i16_v) : int16_v(i16_v){}
		explicit udata64(i8_t const& i8_v) : int8_v(i8_v){}
		explicit udata64( void* const ptr) : ptr_v(ptr) {}

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
	void	srand(unsigned int const& seed = 0);
	u64_t	random_u64(bool const& upseed = false);
	u32_t	random_u32(bool const& upseed = false);
	u16_t	random_u16(bool const& upseed = false);
	u8_t	random_u8(bool const& upseed = false);
}
#endif
