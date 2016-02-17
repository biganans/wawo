#ifndef _WAWO_TASK_TASK_MANAGER_H_
#define _WAWO_TASK_TASK_MANAGER_H_


#include <vector>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/NonCopyable.hpp>

#include <wawo/SafeSingleton.hpp>
#include <wawo/task/TaskRunnerPool.h>
#include <wawo/thread/Thread.h>
#include <wawo/task/Task.hpp>


//#ifdef WAWO_PLATFORM_WIN
	#define WAWO_TASK_MANAGER_USE_SPIN_MUTEX
//#endif

namespace wawo { namespace task {

	class TaskManager:
		public ThreadRunObject_Abstract
	{

	public:
		enum TaskManagerState {
			S_IDLE,
			S_RUN,
			S_CANCELLING, /*this is middle state*/
			S_EXIT,
		};

		TaskManager ( uint32_t const& max_task_runner = WAWO_DEFAULT_TASK_RUNNER_CONCURRENCY_COUNT, bool const& auto_start = false );
		virtual ~TaskManager();
	public:

		// if strap tag given , the task would be assigned to sequence task runner by strap_tag id, strap_id must >= 0
		int PlanTask( WAWO_REF_PTR<SequenceTask> const& task );

		inline int PlanTask( WAWO_REF_PTR<Task_Abstract> const& task ) {

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg(m_tasks_mutex);
#else
			UniqueLock<Mutex> _lg( m_mutex );
#endif
			if( m_state != S_RUN ) {
				//we would hit this case, but the most count less than max_runner_concurrency
				return wawo::E_TASK_MANAGER_INVALID_STATE ;
			}

			m_tasks_to_add->push_back( task ) ;
			if( m_should_notify ) {
				//WAWO_LOG_DEBUG("task_manager", "send notify, current task to add size: %d", m_tasks_to_add->size() );
				m_condition.notify_one();
			}

			WAWO_TRACK_TASK("task_manager", "added tid: %lld, current task to add size: %d", task->GetId(), m_tasks_to_add->size() );
			return wawo::OK ;
		}
		//void CancelTask( WAWO_REF_PTR<Task_Abstract> const& task ); // remove from to exec list , or signal corresponding thread

		inline void SetConcurrency( uint32_t const& max_count ) {
			WAWO_ASSERT( m_runner_pool != NULL );
			m_runner_pool->SetMaxTaskRunner(max_count);
		}

		inline uint32_t CurrentTaskRunnerCount() { return m_runner_pool->CurrentTaskRunnerCount(); }
		inline uint32_t const& GetMaxTaskRunner() {return m_runner_pool->GetMaxTaskRunner();}
		inline uint32_t const& GetMaxTaskRunner() const {return m_runner_pool->GetMaxTaskRunner();}

		void Stop();
		void OnStart();
		void OnStop();
		void Run();

		uint32_t CancelAll() ;

	private:
		SequenceTaskRunnerPool* m_sequence_runners;

		TaskRunnerPool* m_runner_pool;
		TaskVector* m_tasks_to_add;
		TaskVector* m_tasks_to_assign;

		Mutex m_mutex;
		int m_state;

		bool m_should_notify;


#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
		SpinMutex m_tasks_mutex;
		ConditionAny m_condition; //sleep for empty task
#else
		Condition m_condition;
#endif

		uint32_t _CancelAll();
		uint32_t m_assign_cancel_c;
	};

}}
#endif