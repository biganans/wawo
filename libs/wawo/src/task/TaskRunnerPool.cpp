#include <wawo/core.h>
#include <wawo/Task/TaskRunnerPool.h>

namespace wawo { namespace task {

	TaskRunnerPool::TaskRunnerPool(  std::string const& name, int max_runner ):
		m_is_running(false),
		m_last_runner_idx(0)
	{
		WAWO_ASSERT( max_runner <= WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT );

		m_max_runner_count = max_runner ;
		m_pool_name = name;
	}

	TaskRunnerPool::~TaskRunnerPool() {
		Deinit();
	}

	void TaskRunnerPool::Init() {
		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		m_is_running = true ;
		m_last_runner_idx = 0;
	}

	void TaskRunnerPool::Deinit() {

		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		if( m_is_running == false ) { return ; }

		m_is_running = false;
		while( !m_task_runner_list.empty() ) {
			TaskRunner* runner = m_task_runner_list.back();
			WAWO_DELETE( runner ) ;
			m_task_runner_list.pop_back() ;
		}
	}

	int TaskRunnerPool::NewTaskRunner() {

		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		if( !m_is_running ) {
			return wawo::E_TASK_RUNNER_POOL_INVALID_STATE ;
		}

		int size = m_task_runner_list.size() ;

		TaskRunner* taskRunner = new TaskRunner(size) ;
		WAWO_RETURN_V_IF_MATCH( wawo::E_MEMORY_ALLOC_FAILED , taskRunner == NULL );

		int rt = taskRunner->Start();
		if( rt != wawo::OK ) {
			WAWO_DELETE( taskRunner );
			return rt;
		}
		m_task_runner_list.push_back( taskRunner );
		return rt;
	}

	int TaskRunnerPool::AvailableTaskRunnerCount() {
		//currently waiting 
		int waitingTaskRunnerCount = WaitingTaskRunnerCount(); 
			
		//potential task runner
		int potentialTaskRunner = m_max_runner_count - m_task_runner_list.size() ;
		return waitingTaskRunnerCount + potentialTaskRunner ;
	}

	int TaskRunnerPool::WaitingTaskRunnerCount() {
		int waitingTaskRunner = 0;
		TaskRunnerList::iterator it = m_task_runner_list.begin();
		while( it != m_task_runner_list.end() ) {
			
			if( (*it)->IsWaiting() ) {
				++waitingTaskRunner ;
			}
			++it;
		}
		return waitingTaskRunner ;
	}

	uint32_t TaskRunnerPool::CancelAll() {
		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;

		uint32_t total_cancelled = 0;
		std::for_each( m_task_runner_list.begin(), m_task_runner_list.end(), [&]( TaskRunner* const tr ) {
			total_cancelled += tr->CancelAll();
		});

		return total_cancelled;
	}

	int TaskRunnerPool::AssignTask( WAWO_REF_PTR<Task_Abstract> const& task ) {

		WAWO_ASSERT( m_is_running == true );
		uint32_t runner_size = m_task_runner_list.size();
		WAWO_ASSERT( m_last_runner_idx <= runner_size );

		if( m_last_runner_idx == runner_size ) {
			m_last_runner_idx=0;
		}

		while( m_last_runner_idx != runner_size ) {
			if( (m_task_runner_list[m_last_runner_idx])->IsWaiting() ) {
				if((m_task_runner_list[m_last_runner_idx])->AssignTask( task ) == wawo::OK) {
					return wawo::OK;
				}
			}
			++m_last_runner_idx;
		}

		if ( runner_size < m_max_runner_count ) {
			if ( NewTaskRunner() == wawo::OK ) {
				return m_task_runner_list.back()->AssignTask( task ) ;
			}
		}

		return wawo::E_NO_TASK_RUNNER_AVAILABLE;
	}

	void TaskRunnerPool::SetMaxTaskRunner( uint32_t const& count ) { 
		UniqueLock<Mutex> _lg(m_mutex) ;
		m_max_runner_count = count; 

		//dynamic clear ,,if ...
		while( m_task_runner_list.size() > m_max_runner_count ) {
			TaskRunner* runner = m_task_runner_list.back();
			WAWO_DELETE( runner ) ;
			m_task_runner_list.pop_back() ;
		}
	}

	//for sequence task
	SequenceTaskRunnerPool::SequenceTaskRunnerPool( int concurrency_runner ) :
		m_is_running(false),
		m_concurrency(concurrency_runner)
	{
	}

	SequenceTaskRunnerPool::~SequenceTaskRunnerPool() {
		Deinit();
	}

	void SequenceTaskRunnerPool::Init() {
		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		m_is_running = true;

		WAWO_ASSERT( m_task_runner_list.size() == 0 );

		int i = 0 ;
		while( i < m_concurrency ) {
			SequenceTaskRunner* runner = new SequenceTaskRunner(i);
			WAWO_NULL_POINT_CHECK( runner );

			int launch_rt = runner->Start();
			WAWO_CONDITION_CHECK( launch_rt == wawo::OK );

			m_task_runner_list.push_back( runner );
			i++;
		}
	}

	void SequenceTaskRunnerPool::Deinit() {
		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		if( m_is_running == false ) { return ; }

		m_is_running = false;
		while( m_task_runner_list.size() ) {
			SequenceTaskRunner* runner = m_task_runner_list.back();
			WAWO_DELETE( runner ) ;
			m_task_runner_list.pop_back() ;
		}

		WAWO_ASSERT( m_task_runner_list.size() == 0 );
	}

	int SequenceTaskRunnerPool::AssignTask( WAWO_REF_PTR<SequenceTask> const& task ) {
		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;

		if( !m_is_running ) {
			return wawo::E_TASK_RUNNER_POOL_INVALID_STATE ;
		}

		WAWO_ASSERT( task != NULL );
		WAWO_ASSERT( task->Isset() );
		WAWO_ASSERT( task->m_sequence_tag >= 0) ;

		uint32_t runner_idx = (task->m_sequence_tag%m_concurrency) ;
		WAWO_CONDITION_CHECK( runner_idx >= 0 && runner_idx < m_task_runner_list.size() );

		return m_task_runner_list[runner_idx]->AssignTask( task );
	}

}}