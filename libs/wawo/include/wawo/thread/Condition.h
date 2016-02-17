#ifndef _WAWO_THREAD_CONDITION_H_
#define _WAWO_THREAD_CONDITION_H_

#define WAWO_USE_STD_CONDITION


#include <wawo/NonCopyable.hpp>

#ifdef WAWO_USE_STD_CONDITION
	#include <condition_variable>
#endif

namespace wawo { namespace thread {

	namespace _impl_ {
	#ifdef WAWO_USE_STD_CONDITION
			typedef std::mutex cv_mutex;
			template <class _MutexT>
			struct _unique_lock_tpl {
				typedef std::unique_lock<_MutexT> _Type;
			};

			typedef std::adopt_lock_t adopt_lock_t ;
			typedef std::defer_lock_t defer_lock_t ;
			
			typedef std::condition_variable condition_variable;
			typedef std::condition_variable_any condition_variable_any;
	#endif

	} //end of ns _impl_

	template <class _MytexT>
	class UniqueLock;

	namespace _my_wrapper_ {
		
		class condition_variable:
			public wawo::NonCopyable {

		public:
				condition_variable();
				~condition_variable();

				void notify_one() ;
				void notify_all() ;
				void wait( UniqueLock<_impl_::cv_mutex>& ulock ) ;

		private:
			_impl_::condition_variable* m_impl;
		};

		class condition_variable_any 
			: public wawo::NonCopyable 
		{
		public:
			condition_variable_any() ;
			~condition_variable_any() ;

			void notify_one() ; //notify one
			void notify_all(); //notify all

			template <class _MutexT>
			void wait ( _MutexT& mutex ) {
				//std::mutex* std_mutex = static_cast<std::mutex*> (mutex.m_handler) ;
				//WAWO_ASSERT( mutex != NULL );

				WAWO_ASSERT( m_impl != NULL );
				m_impl->wait<_MutexT>( mutex );
			}

		private:
			_impl_::condition_variable_any* m_impl;
		};

	} //end of ns _my_wrapper_
}}

namespace wawo { namespace thread {
	typedef _my_wrapper_::condition_variable Condition;
	typedef _my_wrapper_::condition_variable_any ConditionAny;
}}
#endif //end _WAWO_THREAD_CONDITION_H_