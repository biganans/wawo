#ifndef _WAWO_TASK_TASKRUNNER_HPP_
#define _WAWO_TASK_TASKRUNNER_HPP_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/thread/Mutex.hpp>
#include <wawo/thread/Thread.hpp>
#include <wawo/thread/Condition.hpp>
#include <wawo/task/Task.hpp>

//#define USE_TASK_BUFFER
//#define TASK_BUFFER_SIZE 4

#if WAWO_ISWIN
	#define WAWO_TASK_RUNNER_USE_SPIN_MUTEX
#endif

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
		TaskRunner( u32_t const& runner_id );
		~TaskRunner();

		void Stop();
		void OnStart() ;
		void OnStop() ;
		void Run();

		u32_t Cancel();

		inline int AssignTask( WWRP<Task_Abstract> const& task ) {

#ifdef USE_TASK_BUFFER
	#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			LockGuard<Mutex> _lg( m_mutex ) ;
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
			WAWO_TRACK_TASK("[TRunner][-%d-]task assigned success, TID: %llu", m_runner_id, reinterpret_cast<u64_t>( task.Get() ) );
			m_task_current_assign_index = (m_task_current_assign_index+1)%TASK_BUFFER_SIZE;

			if( m_state == TR_S_WAITING ) {
				m_condition.notify_one();
			}

			return wawo::OK;
#else

	#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg( m_mutex ) ;
	#else
			LockGuard<Mutex> _lg( m_mutex ) ;
	#endif
			if( m_state != TR_S_WAITING ) {
				return wawo::E_TASK_RUNNER_INVALID_STATE ;
			}
			WAWO_ASSERT( m_task == NULL || !m_task->Isset() );
			m_task = task;
			m_state = TR_S_ASSIGNED;

			m_condition.notify_one();
			return wawo::OK ;
#endif
		}


		inline bool IsWaiting() {
#ifdef USE_TASK_BUFFER
			return	(
						((m_state == TR_S_WAITING) && ( ((m_task_current_assign_index+1)%TASK_BUFFER_SIZE) != m_task_next_exec_index ) ) ||
						((m_state == TR_S_RUNNING) && ( ((m_task_current_assign_index+1)%TASK_BUFFER_SIZE) !=m_task_current_exec_index) )
					);
#else
			return m_state == TR_S_WAITING ;
#endif
		}
		inline bool IsRunning() const { return m_state == TR_S_RUNNING ;}
		inline bool IsIdle() const { return m_state == TR_S_IDLE ; }
		inline bool IsEnding() const { return m_state == TR_S_ENDING ; }

	private:
		u32_t _Cancel();

		u32_t m_id ;
		TaskRunnerState m_state;

#ifdef USE_TASK_BUFFER
		WWRP<Task_Abstract> m_task[TASK_BUFFER_SIZE];
		int m_task_current_assign_index;
		int m_task_current_exec_index;
		int m_task_next_exec_index;
//		bool m_task_current_assigned;
#else
		WWRP<Task_Abstract> m_task;
#endif


#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		SpinMutex m_mutex;
		ConditionAny m_condition;
#else
		Mutex m_mutex;
		Condition m_condition;
#endif

	};

	class SequencialTask;
	class SequencialTaskRunner :
		public ThreadRunObject_Abstract
	{
		enum State {
			S_IDLE,
			S_RUN,
			S_CANCELLING,
			S_EXIT
		};

	public:
		SequencialTaskRunner( u32_t const& id) ;
		~SequencialTaskRunner() ;

		inline int AssignTask( WWRP<SequencialTask> const& task ) {
			#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
				LockGuard<SpinMutex> _lg( m_mutex ) ;
			#else
				LockGuard<Mutex> _lg( m_mutex ) ;
			#endif

			if( m_state != S_RUN ) {
				return wawo::E_INVALID_STATE ;
			}

			m_standby->push_back(task);
			if(m_notify) {
				m_condition.notify_one();
			}

			return wawo::OK ;
		}

		u32_t CancelAll() ;

		void Stop();
		void OnStart() ;
		void OnStop() ;
		void Run();

	private:
		u32_t _CancelAll() ;

#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		SpinMutex m_mutex;
		ConditionAny m_condition;
#else
		Mutex m_mutex;
		Condition m_condition;
#endif
		State m_state;
		u32_t m_id;

		SequencialTaskVector* m_standby ;
		SequencialTaskVector* m_assigning;
		bool m_notify:1;
		u32_t m_assign_cancel_c;

	};

}}
#endif