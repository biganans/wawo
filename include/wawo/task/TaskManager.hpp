#ifndef _WAWO_TASK_TASK_MANAGER_HPP_
#define _WAWO_TASK_TASK_MANAGER_HPP_

#include <vector>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/Singleton.hpp>

#include <wawo/task/TaskRunnerPool.hpp>
#include <wawo/thread/Thread.hpp>
#include <wawo/task/Task.hpp>


//#ifdef WAWO_PLATFORM_WIN
	#define WAWO_TASK_MANAGER_USE_SPIN_MUTEX
//#endif

namespace wawo { namespace task {

	enum Priority {
		P_HIGH = 0,
		P_NORMAL,
		P_LOW,
		P_MAX
	};

	struct PriorityTaskQueue {
		TaskVector tasks[P_MAX];
	};

#define PTQ_IS_EMPTY(PTQ) \
	( \
		(PTQ)->tasks[P_HIGH].empty() && \
		(PTQ)->tasks[P_NORMAL].empty() && \
		(PTQ)->tasks[P_LOW].empty() \
	)

#define PTQ_SIZE(PTQ) \
	( \
		(PTQ)->tasks[P_HIGH].empty() + \
		(PTQ)->tasks[P_NORMAL].empty() + \
		(PTQ)->tasks[P_LOW].empty() \
	)

#define PTQ_PUSH_BACK(PTQ,t,p) \
	do { \
		WAWO_ASSERT( (p>=0)&&(p<P_MAX )); \
		(PTQ)->tasks[p].push_back(t); \
	}while(0)

#ifdef _DEBUG
	#define PTQ_P_PUSH_BACK(PTQ,t,p) (WAWO_ASSERT( (p>=0)&&(p<P_MAX )),((PTQ)->tasks[p].push_back(t)))
	#define PTQ_P_SIZE(PTQ,p) (WAWO_ASSERT( (p>=0)&&(p<P_MAX )),((PTQ)->tasks[p].size()))
	#define PTQ_P_GET_VECTOR(PTQ,p,tv) (WAWO_ASSERT( (p>=0)&&(p<P_MAX )),(tv=&((PTQ)->tasks[p])))
#else
	#define PTQ_P_PUSH_BACK(PTQ,t,p) ((PTQ)->tasks[p].push_back(t))
	#define PTQ_P_SIZE(PTQ,p) ((PTQ)->tasks[p].size())
	#define PTQ_P_GET_VECTOR(PTQ,p,tv) ((tv=&((PTQ)->tasks[p])))
#endif

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

		TaskManager ( u32_t const& max_runner_count = WAWO_DEFAULT_TASK_RUNNER_CONCURRENCY_COUNT, u32_t const& max_sequential_runner = WAWO_DEFAULT_SEQUENCIAL_TASK_RUNNER_CONCURRENCY_COUNT, bool const& auto_start = false );
		virtual ~TaskManager();
	public:

		inline int Schedule( WWRP<SequencialTask> const& task ) {
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg(m_mutex);
#else
			UniqueLock<Mutex> _lg( m_mutex );
#endif
			if( m_state != S_RUN ) {
				return wawo::E_TASK_MANAGER_INVALID_STATE ;
			}
			return m_sequencial_runner_pool->AssignTask(task);
		}

		inline int Schedule( WWRP<Task_Abstract> const& task, wawo::task::Priority const& priority = P_NORMAL ) {

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg(m_mutex);
#else
			UniqueLock<Mutex> _lg( m_mutex );
#endif
			if( m_state != S_RUN ) {
				//we would hit this case, but the most count less than max_runner_concurrency
				return wawo::E_TASK_MANAGER_INVALID_STATE ;
			}

			PTQ_P_PUSH_BACK( m_tasks_standby, task, priority );
			//m_tasks_to_add->push_back( task ) ;

			if( m_notify ) {
				//WAWO_DEBUG("[task_manager]send notify, current task to add size: %d", m_tasks_to_add->size() );
				m_condition.notify_one();
			}

			WAWO_TRACK_TASK("[task_manager]current task to add size: %d", m_tasks_to_add->size() );
			return wawo::OK ;
		}

		void SetConcurrency( u32_t const& max ) {
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg(m_mutex);
#else
			UniqueLock<Mutex> _lg( m_mutex );
#endif
			WAWO_ASSERT( m_runner_pool != NULL );
			m_max_concurrency = max;
			m_runner_pool->SetMaxTaskRunner(max);
		}

		void SetSeqConcurrency(u32_t const& max) {
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg(m_mutex);
#else
			UniqueLock<Mutex> _lg(m_mutex);
#endif
			m_max_seq_concurrency = max;
		}


		inline u32_t const& GetMaxTaskRunner() const {return m_runner_pool->GetMaxTaskRunner();}

		void Stop();
		void OnStart();
		void OnStop();
		void Run();

		u32_t CancelAll() ;

	private:
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
		SpinMutex m_mutex;
		ConditionAny m_condition; //sleep for empty task
#else
		Mutex m_mutex;
		Condition m_condition;
#endif

		int m_state;

		TaskRunnerPool* m_runner_pool;
		PriorityTaskQueue* m_tasks_standby;
		PriorityTaskQueue* m_tasks_assigning;
		bool m_notify;

		SequencialTaskRunnerPool* m_sequencial_runner_pool;

		u32_t m_max_concurrency;
		u32_t m_max_seq_concurrency;
		u32_t m_assign_cancel_c;

		u32_t _CancelAll();
	};
}}
#endif