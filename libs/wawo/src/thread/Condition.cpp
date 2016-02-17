#include <condition_variable>
#include <wawo/core.h>
#include <wawo/thread/Condition.h>
#include <wawo/thread/Mutex.h>

namespace wawo { namespace thread {

	namespace _my_wrapper_ {

		condition_variable::condition_variable():
			m_impl(NULL)
		{
			m_impl = new _impl_::condition_variable() ;
			WAWO_NULL_POINT_CHECK( m_impl );
		}
		condition_variable::~condition_variable() {
			WAWO_DELETE( m_impl );
		}

		void condition_variable::notify_one() {
			WAWO_NULL_POINT_CHECK( m_impl );
			m_impl->notify_one();
		}

		void condition_variable::notify_all() {
			WAWO_NULL_POINT_CHECK( m_impl );
			m_impl->notify_all();
		}
		void condition_variable::wait( UniqueLock<_impl_::cv_mutex>& ulock ) {
			WAWO_NULL_POINT_CHECK( m_impl );

			_impl_::cv_mutex* mtx = static_cast<_impl_::cv_mutex*>(ulock.mutex());
			WAWO_ASSERT( mtx != NULL );
			_impl_::defer_lock_t _defer_lock_v;
			_impl_::_unique_lock_tpl<_impl_::cv_mutex>::_Type unique_lock( *mtx, _defer_lock_v );
			m_impl->wait(unique_lock);
		}

		condition_variable_any::condition_variable_any() :
			m_impl(NULL)
		{		
			m_impl = new _impl_::condition_variable_any() ;
			WAWO_NULL_POINT_CHECK( m_impl );
		}

		condition_variable_any::~condition_variable_any() {
			WAWO_DELETE( m_impl );
		}

		void condition_variable_any::notify_one() {
			WAWO_ASSERT( m_impl != NULL );
			m_impl->notify_one();
		}

		void condition_variable_any::notify_all() {
			WAWO_ASSERT( m_impl != NULL );
			m_impl->notify_all();
		}
	} //end of ns _impl_
}}