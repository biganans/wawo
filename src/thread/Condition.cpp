#include <condition_variable>
#include <wawo/core.h>
#include <wawo/thread/Condition.hpp>
#include <wawo/thread/Mutex.hpp>

namespace wawo { namespace thread {

	namespace _my_wrapper_ {

		condition_variable::condition_variable():
			m_impl()
		{
		}
		condition_variable::~condition_variable() {
		}

		void condition_variable::notify_one() {
			m_impl.notify_one();
		}

		void condition_variable::notify_all() {
			m_impl.notify_all();
		}
		void condition_variable::wait( UniqueLock<_my_wrapper_::mutex>& ulock ) {
			_impl_::defer_lock_t _defer_lock_v;
			_impl_::unique_lock<_impl_::cv_mutex>::_Type unique_lock( *(ulock.mutex()->impl()), _defer_lock_v );
			m_impl.wait(unique_lock);
		}

		condition_variable_any::condition_variable_any() :
			m_impl()
		{
		}

		condition_variable_any::~condition_variable_any() {
		}

		void condition_variable_any::notify_one() {
			m_impl.notify_one();
		}

		void condition_variable_any::notify_all() {
			m_impl.notify_all();
		}
	} //end of ns _impl_
}}
