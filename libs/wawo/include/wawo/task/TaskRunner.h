#ifndef _WAWO_TASKRUNNER_H_
#define _WAWO_TASKRUNNER_H_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/NonCopyable.hpp>
#include <wawo/thread/Mutex.h>
#include <wawo/thread/Thread.h>
#include <wawo/thread/Condition.h>
#include <wawo/task/Task.hpp>

#define USE_TASK_BUFFER
#define TASK_BUFFER_SIZE 4

//#ifdef WAWO_PLATFORM_WIN
	#define WAWO_TASK_RUNNER_USE_SPIN_MUTEX
//#endif

namespace wawo { namespace task {
	using namespace wawo::thread;

	enum TaskRunnerState {
		TR_S_IDLE,
		TR_S_WAITING,
		TR_S_ASSIGNED,
		TR_S_RUNNING,
		TR_S_ENDING
	};

	class Task;
	class TaskRunner:
		public ThreadRunObject_Abstract
	{

	public:
		TaskRunner( uint32_t const& runner_id );
		~TaskRunner();

		void Stop();
		void OnStart() ;
		void OnStop() ;
		void Run();

		uint32_t CancelTask(WAWO_REF_PTR<Task_Abstract> const& task);
		uint32_t CancelAll();

		inline int AssignTask( WAWO_REF_PTR<Task_Abstract> const& task ) {

#ifdef USE_TASK_BUFFER

	#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _lg( m_mutex ) ;
	#endif
			if( m_state == TR_S_ENDING ) {
				return wawo::E_TASK_RUNNER_INVALID_STATE ;
			}

			if( (m_task_current_assign_index+1)%TASK_BUFFER_SIZE == m_task_next_exec_index ) {
				m_condition.notify_one();
				return wawo::E_TASK_RUNNER_BUSY;
			}

			WAWO_ASSERT( m_task[m_task_current_assign_index] == NULL || !m_task[m_task_current_assign_index]->Isset() );
			m_task[m_task_current_assign_index] = task;
			WAWO_TRACK_TASK("TRunner", "[-%d-]task assigned success, tid: %d", m_runner_id, task->GetId() );
			m_task_current_assign_index = (m_task_current_assign_index+1)%TASK_BUFFER_SIZE;

			if( m_state == TR_S_WAITING ) {
				m_condition.notify_one();
			}

			return wawo::OK;
#else
			UniqueLock<Mutex> _lg( m_mutex ) ;
			if( m_state != TR_S_WAITING ) {
				return wawo::E_TASK_RUNNER_INVALID_STATE ;
			}

			m_state = TR_S_ASSIGNED;
			m_task = task;
			m_condition.NotifyOne();
			return wawo::OK ;
#endif
		}


		bool IsWaiting() {

#ifdef USE_TASK_BUFFER
			return	( 
						((m_state == TR_S_WAITING) && ( ((m_task_current_assign_index+1)%TASK_BUFFER_SIZE) != m_task_next_exec_index ) ) ||
						((m_state == TR_S_RUNNING) && ( ((m_task_current_assign_index+1)%TASK_BUFFER_SIZE) !=m_task_current_exec_index) )
					);
#else
			return m_state == TR_S_WAITING ;
#endif
		}
		inline bool IsRunning() { return m_state == TR_S_RUNNING ;}
		inline bool IsIdle() { return m_state == TR_S_IDLE ; }
		inline bool IsEnding() { return m_state == TR_S_ENDING ; }

	private:
		uint32_t _CancelAll();

		uint32_t m_runner_id ;
		TaskRunnerState m_state;

#ifdef USE_TASK_BUFFER
		WAWO_REF_PTR<Task_Abstract> m_task[TASK_BUFFER_SIZE];
		int m_task_current_assign_index;
		int m_task_current_exec_index;
		int m_task_next_exec_index;
//		bool m_task_current_assigned;
#else
		WAWO_REF_PTR<Task_Abstract> m_task;
#endif


#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		SpinMutex m_mutex;
		ConditionAny m_condition;
#else
		Mutex m_mutex;
		Condition m_condition;
#endif

	};

	class SequenceTask;
	class SequenceTaskRunner :
		public ThreadRunObject_Abstract
	{
	public:
		SequenceTaskRunner( uint32_t const& sequence) ;
		~SequenceTaskRunner() ;

		int AssignTask( WAWO_REF_PTR<SequenceTask> const& task ) ;

		void Stop();
		void OnStart() ;
		void OnStop() ;
		void Run();

	private:
		SequenceTaskVector* m_to_executing ;
		SequenceTaskVector* m_executing;

		TaskRunnerState m_state;

#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		SpinMutex m_mutex;
		ConditionAny m_condition;
#else
		Mutex m_mutex;
		Condition m_condition;
#endif
		uint32_t m_sequence;
	};
}}

#endif
