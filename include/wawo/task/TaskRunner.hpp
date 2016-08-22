#ifndef _WAWO_TASK_TASKRUNNER_HPP_
#define _WAWO_TASK_TASKRUNNER_HPP_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/thread/Mutex.hpp>
#include <wawo/thread/Thread.hpp>
#include <wawo/thread/Condition.hpp>
#include <wawo/task/Task.hpp>

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
		}

		inline bool IsWaiting() const {
			return m_state == TR_S_WAITING ;
		}
		inline bool IsRunning() const { return m_state == TR_S_RUNNING ;}
		inline bool IsIdle() const { return m_state == TR_S_IDLE ; }
		inline bool IsEnding() const { return m_state == TR_S_ENDING ; }

	private:
		u32_t _Cancel();

		u32_t m_id ;
		TaskRunnerState m_state;
		WWRP<Task_Abstract> m_task;

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