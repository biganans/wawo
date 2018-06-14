#ifndef _WAWO_THREAD_IMPL_MUTEX_HPP_
#define _WAWO_THREAD_IMPL_MUTEX_HPP_

#include <mutex>
#include <wawo/core.hpp>

#include <wawo/thread_impl/mutex_basic.hpp>

namespace wawo {

	namespace impl { typedef std::mutex mutex; }

	namespace _mutex_detail {
		class mutex
		{
			impl::mutex m_impl;

			WAWO_DECLARE_NONCOPYABLE(mutex)
		public:
			mutex() :
				m_impl()
			{
			}
			~mutex() {
				_MUTEX_DEBUG_CHECK_FOR_LOCK_
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
			inline impl::mutex* impl() const { return (impl::mutex*)(&m_impl); }
		};
	}
}

namespace wawo {
#ifdef _WAWO_USE_MUTEX_WRAPPER
	typedef wawo::_mutex_detail::mutex mutex;
#else
	typedef wawo::impl::mutex mutex;
#endif
}

#endif //_WAWO_THREAD_MUTEX_H_