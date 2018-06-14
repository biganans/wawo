#include <wawo/core.hpp>
#include <wawo/mutex.hpp>
#include <wawo/thread.hpp>
#include <wawo/tls.hpp>

#ifdef _DEBUG
	#define _DEBUG_MUTEX
	#define _DEBUG_SHARED_MUTEX
#endif

namespace wawo {

	const adopt_lock_t		adopt_lock;
	const defer_lock_t		defer_lock;
	const try_to_lock_t		try_to_lock;

	namespace impl {

		atomic_spin_mutex::atomic_spin_mutex()
#if defined(WAWO_PLATFORM_GNU) && defined (USE_BOOL_FLAG)
		:m_flag(false)
#endif
		{

#ifdef _WAWO_CHECK_ATOMIC_BOOL
			std::atomic_bool ab;
			bool is_lock_free = ab.is_lock_free();
			std::atomic<bool> abb;
			bool is_lock_free_b = abb.is_lock_free();

			int sizeof_atomic_bool = sizeof( std::atomic_bool );
			int sizeof_atomic_bool_ins = sizeof( ab );

			int sizeof_atomic_bool_my_bool = sizeof( std::atomic<bool> );
			int sizeof_atomic_bool_my_bool_ins = sizeof( abb );
#endif

	#if defined(WAWO_PLATFORM_WIN) && defined(USE_BOOL_FLAG)
			std::atomic_init(&m_flag,false);
	#endif

	#if defined(WAWO_PLATFORM_WIN) && !defined(USE_BOOL_FLAG) && (_MSC_VER<1900)
			const static std::atomic_flag flag = {0};
			m_flag = flag;
	#endif

	#ifdef USE_BOOL_FLAG
			WAWO_ASSERT(m_flag.load(std::memory_order_acquire) == false) ;
	#endif
		}
		atomic_spin_mutex::~atomic_spin_mutex() {
	#ifdef USE_BOOL_FLAG
			WAWO_ASSERT( m_flag.load(std::memory_order_acquire) == false );
	#endif
		}

		void atomic_spin_mutex::lock() {
	#ifdef USE_BOOL_FLAG
			while(m_flag.exchange(true, std::memory_order_acquire));
	#else
			while(!try_lock()) {
				wawo::this_thread::no_interrupt_yield();
			}
	#endif
		}

		void atomic_spin_mutex::unlock() {
	#ifdef USE_BOOL_FLAG
			m_flag.store(false, std::memory_order_release);
	#else
			m_flag.clear(std::memory_order_release);
	#endif
		}

		bool atomic_spin_mutex::try_lock() {
	#ifdef USE_BOOL_FLAG
			bool f = false;
			bool t = true;
			return m_flag.compare_exchange_weak(f,t,std::memory_order_acquire,std::memory_order_acquire);
	#else
			return !m_flag.test_and_set(std::memory_order_acquire);
	#endif
		}

	} //endif impl
}