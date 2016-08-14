
#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>

#include <wawo/task/TaskManager.hpp>
#include <wawo/log/LoggerManager.h>


#define WAWO_MONITOR_TASK_VECTOR_CAPACITY
//#define WAWO_MONITOR_TASK_VECTOR_SIZE
#define WAWO_TASK_VECTOR_INIT_SIZE 4096
//#define WAWO_TASK_VECTOR_MONITOR_SIZE 128


namespace wawo { namespace task {

	//std::atomic<u64_t> Task_Abstract::s_auto_increment_id(0);

	TaskManager::TaskManager( u32_t const& max_runner, u32_t const& max_sequential_runner, bool const& auto_start ) :
		m_mutex(),
		m_condition(),
		m_state( S_IDLE ),
		m_runner_pool(NULL),
		m_tasks_standby(NULL),
		m_tasks_assigning(NULL),
		m_notify(true),
		m_sequencial_runner_pool(NULL),
		m_max_concurrency(max_runner),
		m_max_seq_concurrency(max_sequential_runner),
		m_assign_cancel_c(0)
	{
		if( auto_start ) {
			WAWO_CONDITION_CHECK( Start() == wawo::OK );
		}
	}

	TaskManager::~TaskManager() {
		Stop();

		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_ASSERT( m_tasks_standby == NULL );
		WAWO_ASSERT( m_tasks_assigning == NULL);

		WAWO_ASSERT(m_runner_pool == NULL);
		WAWO_ASSERT( m_sequencial_runner_pool == NULL );

		WAWO_DEBUG("[TaskManager]~TaskManager()");
	}

	void TaskManager::Stop() {
		{
	#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg( m_mutex );
	#else
			UniqueLock<Mutex> _ul( m_mutex );
	#endif
			if( m_state == S_EXIT ) { return; }
			m_state = S_EXIT;

			_CancelAll();
			m_condition.notify_one();
		}

		ThreadRunObject_Abstract::Stop() ;
	}

	void TaskManager::OnStart() {
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> lg( m_mutex );
#else
		UniqueLock<Mutex> _ul( m_mutex );
#endif
		m_state = S_RUN;

		m_tasks_standby = new PriorityTaskQueue();
		WAWO_NULL_POINT_CHECK( m_tasks_standby );
		for(int i=0;i<P_MAX;i++) {
			m_tasks_standby->tasks[i].reserve(WAWO_TASK_VECTOR_INIT_SIZE);
		}

		m_tasks_assigning = new PriorityTaskQueue();
		WAWO_NULL_POINT_CHECK( m_tasks_assigning );
		for(int i=0;i<P_MAX;i++) {
			m_tasks_assigning->tasks[i].reserve(WAWO_TASK_VECTOR_INIT_SIZE);
		}

		WAWO_ASSERT( PTQ_IS_EMPTY( m_tasks_standby ));
		WAWO_ASSERT( PTQ_IS_EMPTY( m_tasks_assigning));

		m_runner_pool = new TaskRunnerPool( m_max_concurrency ) ;
		WAWO_NULL_POINT_CHECK( m_runner_pool ) ;
		m_runner_pool->Init();

		WAWO_ASSERT(m_max_seq_concurrency>0&& m_max_seq_concurrency<128);
		m_sequencial_runner_pool = new SequencialTaskRunnerPool(m_max_seq_concurrency);
		WAWO_NULL_POINT_CHECK( m_sequencial_runner_pool ) ;
		m_sequencial_runner_pool->Init();
	}

	void TaskManager::OnStop() {
		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_ASSERT( PTQ_IS_EMPTY( m_tasks_standby ));
		WAWO_ASSERT( PTQ_IS_EMPTY( m_tasks_assigning));
		m_runner_pool->Deinit();
		m_sequencial_runner_pool->Deinit();

		WAWO_DELETE( m_tasks_standby );
		WAWO_DELETE( m_tasks_assigning );

		WAWO_DELETE( m_runner_pool );
		WAWO_DELETE( m_sequencial_runner_pool );
	}

	void TaskManager::Run() {
		{

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg( m_mutex );
#else
			UniqueLock<Mutex> _ul( m_mutex );
#endif
			if( m_state == S_EXIT ) {
				return ;
			}
			WAWO_ASSERT( PTQ_IS_EMPTY( m_tasks_assigning));

			while( PTQ_IS_EMPTY(m_tasks_standby) ) {
				m_notify = true;

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
				m_condition.wait<SpinMutex>(m_mutex);
#else
				m_condition.wait(_ul);
#endif
				m_notify=false;
				if ( m_state == S_EXIT ) {
					return ;
				}
				WAWO_ASSERT( m_state == S_RUN );
			}

			WAWO_ASSERT( !(PTQ_IS_EMPTY(m_tasks_standby)) );
			WAWO_ASSERT( PTQ_IS_EMPTY(m_tasks_assigning) );

#ifdef WAWO_MONITOR_TASK_VECTOR_SIZE
			if( m_tasks_to_add->size() > WAWO_TASK_VECTOR_MONITOR_SIZE ) {
				WAWO_WARN("[task_manager]m_tasks_to_add.size() reach: %d", m_tasks_to_add->size() );
			}
#endif
			wawo::swap( m_tasks_standby, m_tasks_assigning );
			WAWO_ASSERT( (PTQ_IS_EMPTY(m_tasks_standby)) );
			WAWO_ASSERT( !PTQ_IS_EMPTY(m_tasks_assigning) );
		}

		for( int pi=0;pi<P_MAX;pi++ ) {

			TaskVector* tv;
			PTQ_P_GET_VECTOR(m_tasks_assigning,pi,tv);
			u32_t tv_c = tv->size();

			u32_t i =0;
			u32_t ntrt = 0;

			while( m_state == S_RUN && i != tv_c ) {
				int rt = m_runner_pool->AssignTask( (*tv)[i] );
				if( rt == wawo::OK ) {
					//WAWO_DEBUG( "[task_manager]assign task success: tid: %lld ", (*m_tasks_to_assign)[i]->GetId() );
					++i ;
					ntrt = 0;
				} else {
					if( rt == wawo::E_NO_TASK_RUNNER_AVAILABLE ) {
						wawo::yield(ntrt++);
					} else {
						WAWO_FATAL( "[task_manager]assign task failed, error: %d", rt );
					}
				}
			}

			u32_t cancelled = (tv_c -i);
			m_assign_cancel_c += cancelled;

			if( (m_state != S_RUN) && i != cancelled ) {
				for( u32_t j=i;j<tv_c;j++ ) {
					((*tv)[j])->Cancel();
					((*tv)[j])->Reset();
				}
				WAWO_WARN("[task_manager]priority: %d , cancelled: %d, for reason not in S_RUN state", pi,cancelled );
			}

			if( (m_state == S_RUN) && (tv_c > WAWO_TASK_VECTOR_INIT_SIZE) ) {
	#ifdef WAWO_MONITOR_TASK_VECTOR_CAPACITY
				WAWO_WARN("[task_manager]m_tasks_assigning[%d].size() == %d, default is : %d, resize to default", pi, tv_c, WAWO_TASK_VECTOR_INIT_SIZE );
	#endif
				tv->resize(WAWO_TASK_VECTOR_INIT_SIZE);
			}
			tv->clear();
		}
	}

	u32_t TaskManager::CancelAll() {
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> _lg(m_mutex);
#else
		LockGuard<Mutex> lg(m_mutex);
#endif
		return _CancelAll();
	}

	u32_t TaskManager::_CancelAll() {

		int _previous_state = m_state;
		m_state = S_CANCELLING;

		u32_t total = m_sequencial_runner_pool->CancelAll() ;

		for( int pi=0;pi<P_MAX;pi++ ) {
			TaskVector* tv;
			PTQ_P_GET_VECTOR(m_tasks_standby,pi,tv);
			std::for_each( tv->begin(), tv->end(),[&total]( WWRP<Task_Abstract> const& task ){
				task->Cancel();
				task->Reset();
				++total ;
			});
			tv->clear();
		}
		while( !PTQ_IS_EMPTY(m_tasks_assigning) );
		total += m_assign_cancel_c;
		m_assign_cancel_c = 0;
		m_state = _previous_state ;

		WAWO_DEBUG("[task_manager]CancelAll, cancelled total: %d", total );
		return total;
	}

}}//end of ns
