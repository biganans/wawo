#ifndef _WAWO_THREAD_CONDITION_HPP_
#define _WAWO_THREAD_CONDITION_HPP_

#include <condition_variable>
#include <wawo/core.h>

namespace wawo { namespace thread {

	namespace _my_wrapper_ {
		class mutex;
	}

	namespace _impl_ {
		typedef std::mutex cv_mutex;

		template <class cv_mutex_t>
		struct unique_lock {
			typedef std::unique_lock<cv_mutex_t> _Type;
		};

		typedef std::adopt_lock_t adopt_lock_t ;
		typedef std::defer_lock_t defer_lock_t ;

		typedef std::condition_variable condition_variable;
		typedef std::condition_variable_any condition_variable_any;
	} //end of ns _impl_

	template <class _MutexT>
	class UniqueLock;

	namespace _my_wrapper_ {

		class condition_variable:
			public wawo::NonCopyable {

		public:
			condition_variable();
			~condition_variable();

			void notify_one() ;
			void notify_all() ;
			void wait( UniqueLock<_my_wrapper_::mutex>& ulock ) ;
		private:
			_impl_::condition_variable m_impl;
		};

		class condition_variable_any
			: public wawo::NonCopyable
		{
		public:
			condition_variable_any() ;
			~condition_variable_any() ;

			void notify_one(); //notify one
			void notify_all(); //notify all

			template <class _MutexT>
			inline void wait ( _MutexT& mutex ) {
				m_impl.wait<_MutexT>( mutex );
			}
		private:
			_impl_::condition_variable_any m_impl;
		};

	} //end of ns _my_wrapper_
}}

namespace wawo { namespace thread {
	typedef _my_wrapper_::condition_variable Condition;
	typedef _my_wrapper_::condition_variable_any ConditionAny;
}}
#endif //end _WAWO_THREAD_CONDITION_H_