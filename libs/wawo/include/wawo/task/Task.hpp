#ifndef _WAWO_TASK_TASK_HPP_
#define _WAWO_TASK_TASK_HPP_

#include <atomic>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/thread/Thread.h>
#include <wawo/time/time.hpp>

namespace wawo { namespace task {
	using namespace wawo::thread;

	enum Priority {
		P_LOW = 1,
		P_NORMAL,
		P_HIGH,
		P_SUPER_HIGH
	};

	// interface for task
	class Task_Abstract:
		public wawo::RefObject_Abstract
	{
		uint64_t m_id;
		Priority m_priority:8;

		static std::atomic<uint64_t> s_auto_increment_id ;
		static uint64_t MakeID() {
			return ++s_auto_increment_id ;
		}

	public:
		inline static bool PriorityCompare( WAWO_REF_PTR<Task_Abstract> const& first, WAWO_REF_PTR<Task_Abstract> const& second ) {
			return (first->m_priority == second->m_priority) ? (first->m_id < second->m_id) : (first->m_priority > second->m_priority);
			//if( first->m_priority == second->m_priority ) {
			//	return first->m_id < second->m_id ; //ASC
			//} else {
			//	return first->m_priority > second->m_priority ; //DESC
			//}
		}

		Task_Abstract( Priority const& priority ):
			m_id(MakeID()),
			m_priority(priority)
		{
		}
		virtual ~Task_Abstract() {
		}
		inline uint64_t& GetId() { return m_id; }

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
		explicit Task( WAWO_TASK_FUNC const& run, void* caller, void* param, WAWO_TASK_FUNC const& cancel, Priority const& priority = P_NORMAL ):
			Task_Abstract(priority),
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
	typedef std::function<void()> WAWO_TASK_STD_FUNC ;

	class StdFunctionTask:
		public Task_Abstract {

		WAWO_TASK_STD_FUNC m_std_run_func ;
		WAWO_TASK_STD_FUNC m_std_cancel_func;

		explicit StdFunctionTask( WAWO_TASK_STD_FUNC const& run, WAWO_TASK_STD_FUNC const& cancel,Priority const& priority = P_NORMAL ) :
			Task_Abstract(priority),
			m_std_run_func(run),
			m_std_cancel_func(cancel)
		{
		}

		bool Isset() {
			return m_std_run_func != nullptr;
		}

		void Reset() {
			m_std_run_func = nullptr;
		}

		void Run() {
			WAWO_ASSERT( Isset() );
			m_std_run_func();
		}

		void Cancel() {
			if( m_std_cancel_func != nullptr ) {
				m_std_cancel_func();
			}
		}
	};

	class SequenceTask:
		public Task {
	public:
		explicit SequenceTask( WAWO_TASK_FUNC const& func, void* caller, void* param, int const& sequence_tag, WAWO_TASK_FUNC const& cancel ):
			Task(func,caller,param,cancel,P_NORMAL),
			m_sequence_tag(sequence_tag)
		{
		}

		~SequenceTask() {
			Reset();
		}

		void Reset() {
			m_sequence_tag = 0 ;
			Task::Reset();
		}

		int m_sequence_tag;
	};

	typedef std::vector< WAWO_REF_PTR<Task_Abstract> > TaskVector ;
	typedef std::vector< WAWO_REF_PTR<SequenceTask> > SequenceTaskVector;
}}
#endif
