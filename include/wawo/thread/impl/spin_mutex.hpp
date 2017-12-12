#ifndef WAWO_THREAD_IMPL_SPIN_MUTEX_HPP_
#define WAWO_THREAD_IMPL_SPIN_MUTEX_HPP_

#include <atomic>
#include <wawo/core.hpp>

#ifdef WAWO_PLATFORM_GNU
	#define _WAWO_USE_PTHREAD_SPIN_AS_SPIN_MUTEX
	#include <pthread.h>
#endif

#include <wawo/thread/impl/mutex_basic.hpp>

namespace wawo { namespace thread {

	namespace impl {

		class atomic_spin_mutex {
			WAWO_DECLARE_NONCOPYABLE(atomic_spin_mutex)
#ifdef USE_BOOL_FLAG
				std::atomic_bool m_flag; //ATOMIC_VAR_INIT(false);
#else
#if defined(WAWO_PLATFORM_GNU) || (_MSC_VER>=1900)
				std::atomic_flag m_flag = ATOMIC_FLAG_INIT; //force for sure
#else
				std::atomic_flag m_flag;
#endif
#endif
		public:
			atomic_spin_mutex();
			~atomic_spin_mutex();

			void lock();
			void unlock();
			bool try_lock();
		};

#ifdef WAWO_PLATFORM_GNU
		class pthread_spin_mutex {

			WAWO_DECLARE_NONCOPYABLE(pthread_spin_mutex)
				pthread_spinlock_t m_spin;
		public:
			pthread_spin_mutex() :
				m_spin()
			{
				pthread_spin_init(&m_spin, 0);
			}
			~pthread_spin_mutex() {
				pthread_spin_destroy(&m_spin);
			}
			void lock() {
				pthread_spin_lock(&m_spin);
			}
			void unlock() {
				pthread_spin_unlock(&m_spin);
			}
			bool try_lock() {
				return pthread_spin_trylock(&m_spin) == 0;
			}
		};
#endif

#ifdef _WAWO_USE_PTHREAD_SPIN_AS_SPIN_MUTEX
		typedef pthread_spin_mutex spin_mutex;
#else
		typedef atomic_spin_mutex spin_mutex;
#endif

	}

	namespace _mutex_detail {
		class spin_mutex {
		public:
			impl::spin_mutex m_impl;
			WAWO_DECLARE_NONCOPYABLE(spin_mutex)
		public:
			spin_mutex() :
				m_impl()
			{
			}
			~spin_mutex() {
			}
			inline void lock() {
				_MUTEX_IMPL_LOCK(m_impl);
			}
			inline void unlock() {
				_MUTEX_IMPL_UNLOCK(m_impl);
			}
			inline bool try_lock() {
				_MUTEX_IMPL_TRY_LOCK(m_impl);
			}
			inline impl::spin_mutex* impl() const { return (impl::spin_mutex*)&m_impl; }
		};
	}
}}

namespace wawo { namespace thread {
#ifdef _WAWO_USE_MUTEX_WRAPPER
		typedef wawo::thread::_mutex_detail::spin_mutex spin_mutex;
#else
		typedef wawo::thread::impl::spin_mutex spin_mutex;
#endif
}}

#endif