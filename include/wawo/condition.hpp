#ifndef _WAWO_CONDITION_HPP_
#define _WAWO_CONDITION_HPP_

#include <condition_variable>

#include <wawo/core.hpp>
#include <wawo/thread_impl/mutex.hpp>
#include <wawo/thread.hpp>

namespace wawo {

	const std::chrono::hours __years_10__(24*365*10);
	const std::chrono::hours __years_1__(24*365);
	const std::chrono::hours __months_6__((24*365)>>1);
	const std::chrono::hours __months_1__((24 * 30));
	const std::chrono::hours __week_1__(24*7);
	const std::chrono::hours __day_1__(24);

	namespace impl {
		template <class mutex_t>
		struct interruption_checker {

			WAWO_DECLARE_NONCOPYABLE(interruption_checker)
		public:
			impl::thread_data* const thdata;
			mutex_t* m;
			u8_t t;

			explicit interruption_checker(mutex_t* mtx, condition_variable* cond) :
				thdata(wawo::tls_get<thread_data>()),
				m(mtx),
				t(0)
			{
				if (thdata) {
					lock_guard<spin_mutex> lg(thdata->mtx);
					if (thdata->interrupted) {
						thdata->interrupted = false;
						throw impl::interrupt_exception();
					}
					thdata->current_cond = cond;
					thdata->current_cond_mutex = mtx;
					m->lock();
				}
				else {
					m->lock();
				}
			}

			explicit interruption_checker(mutex_t* mtx, condition_variable_any* cond) :
				thdata(wawo::tls_get<thread_data>()),
				m(mtx),
				t(1)
			{
				if (thdata) {
					lock_guard<spin_mutex> lg(thdata->mtx);
					if (thdata->interrupted) {
						thdata->interrupted = false;
						throw impl::interrupt_exception();
					}
					thdata->current_cond_any = cond;
					thdata->current_cond_any_mutex = mtx;
					m->lock();
				}
				else {
					m->lock();
				}
			}

			~interruption_checker() {
				if (thdata) {
					WAWO_ASSERT(m != NULL);
					m->unlock();
					lock_guard<spin_mutex> lg(thdata->mtx);

					if (t == 0) {
						thdata->current_cond_mutex = NULL;
						thdata->current_cond = NULL;
					}
					else {
						thdata->current_cond_any_mutex = NULL;
						thdata->current_cond_any = NULL;
					}
				}
				else {
					m->unlock();
				}
			}
		};

		template <typename mutex_t>
		struct lock_on_exit {
			mutex_t* m;
			lock_on_exit() :
				m(0)
			{}
			void activate(mutex_t& m_)
			{
				m_.unlock();
				m = &m_;
			}
			~lock_on_exit() {
				if (m) { m->lock(); }
			}
		};
	}

	template <class _MutexT>
	class unique_lock;

	enum class cv_status {
		no_timeout = (cv_status)std::cv_status::no_timeout,
		timeout = (cv_status)std::cv_status::timeout
	};

	class condition_variable {
		impl::mutex m_mtx;
		impl::condition_variable m_impl;
		WAWO_DECLARE_NONCOPYABLE(condition_variable)
	public:
		condition_variable() {}
		~condition_variable() {}

		inline void notify_one() {
			scoped_lock<impl::mutex> _internal_lock(&m_mtx);
			m_impl.notify_one();
		}
		inline void no_interrupt_notify_one() {
			m_impl.notify_one();
		}
		inline void notify_all() {
			scoped_lock<impl::mutex> _internal_lock(&m_mtx);
			m_impl.notify_all();
		}
		inline void no_interrupt_notify_all() {
			m_impl.notify_all();
		}

		template <class _Rep, class _Period>
		inline cv_status no_interrupt_wait_for(unique_lock<mutex>& ulock, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::defer_lock_t _defer_lock_v;
			impl::unique_lock<impl::cv_mutex>::type unique_lock(*(ulock.mutex().impl()), _defer_lock_v);
			impl::cv_status cvs = m_impl.wait_for(unique_lock, duration);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _Clock, class _Duration>
		cv_status no_interrupt_wait_until(unique_lock<mutex>& ulock, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::defer_lock_t _defer_lock_v;
			impl::unique_lock<impl::cv_mutex>::type implulk( *(ulock.mutex().impl()), _defer_lock_v);
			impl::cv_status cvs = m_impl.wait_until(implulk, atime);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		void no_interrupt_wait(unique_lock<mutex>& ulock) {
			impl::defer_lock_t _defer_lock_v;
			impl::unique_lock<impl::cv_mutex>::type ulk(*(ulock.mutex().impl()), _defer_lock_v);
			m_impl.wait(ulk);
		}
		void wait(unique_lock<mutex>& ulock) {
			{
				impl::defer_lock_t _defer_lock_v;
				impl::lock_on_exit<unique_lock<mutex>> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock);
				impl::unique_lock<impl::cv_mutex>::type self_implulk(m_mtx, _defer_lock_v);
				m_impl.wait(self_implulk);
			}
			wawo::this_thread::__interrupt_check_point();
		}

		template <class _Rep, class _Period>
		inline cv_status wait_for(unique_lock<mutex>& ulock, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs;
			{
				impl::defer_lock_t _defer_lock_v;
				impl::lock_on_exit<unique_lock<mutex>> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock);

				impl::unique_lock<impl::cv_mutex>::type self_implulk(m_mtx, _defer_lock_v);
				cvs = m_impl.wait_for(self_implulk, duration);
			}
			wawo::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _Clock, class _Duration>
		cv_status wait_until(unique_lock<mutex>& ulock, std::chrono::time_point<_Clock, _Duration> const& atime ) {
			impl::cv_status cvs;
			{
				impl::defer_lock_t _defer_lock_v;
				impl::lock_on_exit<unique_lock<mutex>> guard;
				impl::interruption_checker<impl::mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(ulock);

				impl::unique_lock<impl::cv_mutex>::type self_implulk(m_mtx, _defer_lock_v);
				cvs = m_impl.wait_until(self_implulk, atime);
			}
			wawo::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}
	};

	class condition_variable_any
	{
		impl::spin_mutex m_mtx;
		impl::condition_variable_any m_impl;
		WAWO_DECLARE_NONCOPYABLE(condition_variable_any)
	public:
		condition_variable_any() {}
		~condition_variable_any() {}

		inline void notify_one() {
			scoped_lock<impl::spin_mutex> sl(&m_mtx);
			m_impl.notify_one();
		}
		inline void no_interrupt_notify_one() {
			m_impl.notify_one();
		}
		inline void notify_all() {
			scoped_lock<impl::spin_mutex> sl(&m_mtx);
			m_impl.notify_all();
		}
		inline void no_interrupt_notify_all() {
			m_impl.notify_all();
		}

		template <class _MutexT>
		inline void no_interrupt_wait(_MutexT& mutex) {
			m_impl.wait(mutex);
		}

		template <class _MutexT, class _Rep, class _Period>
		inline cv_status no_interrupt_wait_for(_MutexT& mutex, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs = m_impl.wait_for(mutex, duration);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _MutexT, class _Clock, class _Duration>
		inline cv_status no_interrupt_wait_until(_MutexT& mutex, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::cv_status cvs = m_impl.wait_until(mutex, atime);
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		template <class _MutexT>
		inline void wait( _MutexT& mtx ) {
			{
				impl::lock_on_exit<_MutexT> guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				m_impl.wait(m_mtx);
			}
			wawo::this_thread::__interrupt_check_point();
		}

		template <class _MutexT, class _Rep, class _Period>
		inline cv_status wait_for(_MutexT& mtx, std::chrono::duration<_Rep, _Period> const& duration) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit<_MutexT> guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				cvs = m_impl.wait_for(m_mtx, duration);
			}
			wawo::this_thread::check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}

		//template <class _MutexT>
		//inline cv_status  wait_for(_MutexT& mtx, u64_t const& timeinnano) {
		//	return wait_for(mtx, std::chrono::nanoseconds(timeinnano));
		//}

		template <class _MutexT, class _Clock, class _Duration>
		cv_status wait_until(_MutexT& mtx, std::chrono::time_point<_Clock, _Duration> const& atime) {
			impl::cv_status cvs;
			{
				impl::lock_on_exit< _MutexT > guard;
				impl::interruption_checker<impl::spin_mutex> _interruption_checker(&m_mtx, &m_impl);
				guard.activate(mtx);

				cvs = m_impl.wait_until(m_mtx, atime);
			}
			wawo::this_thread::__interrupt_check_point();
			return cvs == std::cv_status::no_timeout ? cv_status::no_timeout : cv_status::timeout;
		}
	};

	typedef condition_variable condition;
	typedef condition_variable_any condition_any;
}

#endif //end _WAWO_THREAD_CONDITION_H_