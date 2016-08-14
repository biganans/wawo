#ifndef _WAWO_TASK_TASK_HPP_
#define _WAWO_TASK_TASK_HPP_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>

namespace wawo { namespace task {
	using namespace wawo::thread;

	// interface for task
	class Task_Abstract:
		virtual public wawo::RefObject_Abstract
	{
	public:
		Task_Abstract() {}
		virtual ~Task_Abstract() {}

		virtual bool Isset() = 0;
		virtual void Reset() = 0;
		virtual void Run() = 0;
		virtual void Cancel() = 0;
	};

	typedef void (*WAWO_TASK_FUNC) (void* caller, void* param);
	class Task :
		public Task_Abstract {

	private:
		void* m_caller ;
		void* m_param;
		WAWO_TASK_FUNC m_run_func ;
		WAWO_TASK_FUNC m_cancel_func;

	public:
		explicit Task( WAWO_TASK_FUNC const& run, void* caller, void* param, WAWO_TASK_FUNC const& cancel ):
			Task_Abstract(),
			m_caller(caller),
			m_param(param),
			m_run_func(run),
			m_cancel_func(cancel)
		{
		}

		~Task() {
			Reset();
		}

		bool Isset() {
			return m_run_func != NULL ;
		}

		void Reset() {
			m_caller = 0;
			m_param = 0;
			m_run_func = 0;
			m_cancel_func = 0;
		}

		//for run
		void Run() {
			WAWO_ASSERT( Isset() );
			m_run_func( m_caller, m_param ) ;
		}

		//task cancelled
		void Cancel() {
			if( m_cancel_func != NULL ) {
				m_cancel_func( m_caller, m_param ) ;
			}
		}
	};

	#include <functional>
	typedef std::function<void ()> LambdaFn ;

	class LambdaTask:
		public Task_Abstract {

		LambdaFn m_run_fn ;
		LambdaFn m_cancel_fn;
	public:
		explicit LambdaTask( LambdaFn const& run, LambdaFn const& cancel = nullptr ) :
			Task_Abstract(),
			m_run_fn(run),
			m_cancel_fn(cancel)
		{
		}

		bool Isset() {
			return m_run_fn != nullptr;
		}

		void Reset() {
			m_run_fn = nullptr;
		}

		void Run() {
			WAWO_ASSERT( Isset() );
			m_run_fn();
		}

		void Cancel() {
			if( m_cancel_fn != nullptr ) {
				m_cancel_fn();
			}
		}
	};

	class SequencialTask:
		public Task_Abstract {

	private:
		WWRP<Task_Abstract> m_task;
		u32_t m_tag;
	public:
		explicit SequencialTask( WWRP<Task_Abstract> const& task, u32_t const& tag ):
			m_task(task),
			m_tag(tag)
		{
		}

		~SequencialTask() {
			Reset();
		}

		bool Isset() {
			if (m_task == NULL) {
				return false;
			}
			return m_task->Isset();
		}

		void Reset() {
			if( m_task == NULL) {
				return;
			}
			m_task->Reset();
			m_task = NULL;
		}

		void Run() {
			WAWO_ASSERT( Isset() );
			m_task->Run();
		}

		void Cancel() {
			WAWO_ASSERT( Isset() );
			m_task->Cancel();
		}

		inline u32_t const& GetTag() const {
			return m_tag;
		}
	};

	typedef std::vector< WWRP<Task_Abstract> > TaskVector ;
	typedef std::vector< WWRP<SequencialTask> > SequencialTaskVector;
}}
#endif