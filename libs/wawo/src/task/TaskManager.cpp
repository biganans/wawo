
#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>

#include <wawo/task/TaskManager.h>
#include <wawo/log/LoggerManager.h>


#define WAWO_MONITOR_TASK_VECTOR_CAPACITY
//#define WAWO_MONITOR_TASK_VECTOR_SIZE
#define WAWO_TASK_VECTOR_INIT_SIZE 4096
//#define WAWO_TASK_VECTOR_MONITOR_SIZE 128


namespace wawo { namespace task {

	std::atomic<uint64_t> Task_Abstract::s_auto_increment_id(0);

	TaskManager::TaskManager( uint32_t const& max_task_runner, bool const& auto_start ) :
		m_state( S_IDLE ),
		m_should_notify(true),
		m_condition(),
		m_assign_cancel_c(0)
	{
		m_tasks_to_add = new TaskVector();
		WAWO_NULL_POINT_CHECK( m_tasks_to_add );
		m_tasks_to_add->reserve( WAWO_TASK_VECTOR_INIT_SIZE );

		m_tasks_to_assign = new TaskVector();
		WAWO_NULL_POINT_CHECK( m_tasks_to_assign );
		m_tasks_to_assign->reserve( WAWO_TASK_VECTOR_INIT_SIZE );

		m_runner_pool = new TaskRunnerPool("task_runner_pool", max_task_runner ) ;
		WAWO_NULL_POINT_CHECK( m_runner_pool ) ;

		m_sequence_runners = new SequenceTaskRunnerPool(2) ;
		WAWO_NULL_POINT_CHECK( m_sequence_runners ) ;

		if( auto_start ) {
			WAWO_CONDITION_CHECK( Start() == wawo::OK );
		}
	}

	TaskManager::~TaskManager() {
		Stop();

		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_DELETE(m_tasks_to_add);
		WAWO_DELETE(m_tasks_to_assign);

		WAWO_DELETE(m_sequence_runners);
		WAWO_DELETE(m_runner_pool);

		WAWO_LOG_DEBUG("TaskManager","~TaskManager()");
	}

	void TaskManager::Stop() {
		{
			UniqueLock<Mutex> _lg( m_mutex ) ;
			m_state = S_EXIT;
			_CancelAll();
			m_condition.notify_one();
		}

		ThreadRunObject_Abstract::Stop() ;
	}

	void TaskManager::OnStart() {
		UniqueLock<Mutex> _lg( m_mutex );
		m_state = S_RUN;

		WAWO_ASSERT(m_tasks_to_add->empty());
		WAWO_ASSERT(m_tasks_to_assign->empty()) ;

		m_runner_pool->Init();
		m_sequence_runners->Init();
	}

	void TaskManager::OnStop() {
		UniqueLock<Mutex> _lg( m_mutex );

		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_CONDITION_CHECK(m_tasks_to_add->empty());
		WAWO_CONDITION_CHECK(m_tasks_to_assign->empty());

		m_runner_pool->Deinit() ;
		m_sequence_runners->Deinit();
	}

	int TaskManager::PlanTask( WAWO_REF_PTR<SequenceTask> const& task ) {

		UniqueLock<Mutex> _lg( m_mutex );
		if( m_state != S_RUN ) {
			return wawo::E_TASK_MANAGER_INVALID_STATE ;
		}

		return m_sequence_runners->AssignTask(task);
	}

	/*
	void TaskManager::CancelTask( WAWO_REF_PTR<Task_Abstract> const& task ) {
		task->Cancel();
	}
	*/
	void TaskManager::Run() {
		{

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg( m_tasks_mutex);
#else
			UniqueLock<Mutex> _ul( m_mutex );
#endif
			if( m_state == S_EXIT ) {
				return ;
			}

			WAWO_ASSERT( m_tasks_to_assign->empty() );
			while( m_tasks_to_add->empty() ) {
				m_should_notify = true;
#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
				m_condition.wait<SpinMutex>(m_tasks_mutex);
#else
				m_condition.wait(_ul);
#endif
				m_should_notify=false;
				if ( m_state == S_EXIT ) {
					return ;
				}
			}

			WAWO_ASSERT( !m_tasks_to_add->empty() );
			WAWO_ASSERT( m_tasks_to_assign->empty() );

#ifdef WAWO_MONITOR_TASK_VECTOR_SIZE
			if( m_tasks_to_add->size() > WAWO_TASK_VECTOR_MONITOR_SIZE ) {
				WAWO_LOG_WARN("task_manager", "m_tasks_to_add.size() reach: %d", m_tasks_to_add->size() );
			}
#endif
			wawo::swap( m_tasks_to_assign, m_tasks_to_add );
			WAWO_ASSERT( !m_tasks_to_assign->empty() );
			WAWO_ASSERT( m_tasks_to_add->empty() );
		}

		uint32_t assign_c = m_tasks_to_assign->size(); //total to assign
		//WAWO_LOG_DEBUG( "task_manager", "to assign task count: %d", assign_c );

		uint32_t i = 0;
		uint32_t ntrt = 0; //no task runner timer
		while( m_state == S_RUN && i != assign_c ) {

			int rt = m_runner_pool->AssignTask( (*m_tasks_to_assign)[i] );
			if( rt == wawo::OK ) {
				//WAWO_LOG_DEBUG( "task_manager", "assign task success: tid: %lld ", (*m_tasks_to_assign)[i]->GetId() );
				++i ;
				ntrt = 0;
			} else {

				if( rt == wawo::E_NO_TASK_RUNNER_AVAILABLE ) {
					//WAWO_LOG_WARN( "task_manager", "assign task failed: %d", rt );
					wawo::yield(ntrt++);
				} else {
					WAWO_LOG_FATAL( "task_manager", "assign task failed, error: %d", rt );
				}
			}
		}

		//WAWO_LOG_DEBUG( "task_manager", "assigned task count: %d", i );
		m_assign_cancel_c = assign_c - i;
		if( (m_state != S_RUN) && i != assign_c ) {
			for( uint32_t j=i;j<assign_c;j++ ) {
				((*m_tasks_to_assign)[j])->Cancel();
				((*m_tasks_to_assign)[j])->Reset();
			}
			WAWO_LOG_WARN("task_manager", "m_assign_cancel_c: %d, in total to assign: %d", m_assign_cancel_c,assign_c );
		}

		if( (m_state == S_RUN) && (assign_c > WAWO_TASK_VECTOR_INIT_SIZE) ) {
#ifdef WAWO_MONITOR_TASK_VECTOR_CAPACITY
			WAWO_LOG_WARN("task_manager", "m_tasks_to_assign->capacity() reach: %d, m_tasks_to_assign->size(): %d", m_tasks_to_assign->capacity(),assign_c );
#endif
			m_tasks_to_assign->resize(WAWO_TASK_VECTOR_INIT_SIZE);
		}
		m_tasks_to_assign->clear();
	}

	uint32_t TaskManager::CancelAll() {
		UniqueLock<Mutex> _lgg( m_mutex );
		return _CancelAll();
	}

	uint32_t TaskManager::_CancelAll() {

#ifdef WAWO_TASK_MANAGER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> _lg(m_tasks_mutex);
#endif
		int _previous_state = m_state;

		m_state = S_CANCELLING;

		uint32_t total = 0;
		int i= m_tasks_to_add->size();
		if( i>0 ) {
			std::for_each( m_tasks_to_add->begin(), m_tasks_to_add->end(),[&]( WAWO_REF_PTR<Task_Abstract> const& task ){
				task->Cancel();
				task->Reset();
				++total ;
			});
		}
		m_tasks_to_add->clear();
		while( !m_tasks_to_assign->empty());
		total += m_assign_cancel_c;
		m_assign_cancel_c = 0;

		//total += m_runner_pool->CancelAll();
		WAWO_LOG_WARN("task_manager", "CancelAll, cancelled total: %d", total );

		m_state = _previous_state ;
		return total;
	}

}}//end of ns
