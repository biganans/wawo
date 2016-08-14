#include <wawo/core.h>
#include <wawo/task/TaskRunnerPool.hpp>

namespace wawo { namespace task {

	TaskRunnerPool::TaskRunnerPool( int const& max_runner ):
		m_mutex(),
		m_is_running(false),
		m_max_concurrency(max_runner),
		m_runners(),
		m_last_runner_idx(0)

	{
		WAWO_ASSERT( max_runner <= WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT );
	}

	TaskRunnerPool::~TaskRunnerPool() {
	}

	void TaskRunnerPool::Init() {
		wawo::thread::LockGuard<Mutex> _lg(m_mutex) ;
		m_is_running = true ;
		m_last_runner_idx = 0;
	}

	void TaskRunnerPool::Deinit() {

		wawo::thread::LockGuard<Mutex> _lg(m_mutex) ;
		if( m_is_running == false ) { return ; }

		m_is_running = false;
		while( !m_runners.empty() ) {
			TaskRunner* r = m_runners.back();
			WAWO_DELETE( r ) ;
			m_runners.pop_back() ;
		}
	}

	int TaskRunnerPool::_NewTaskRunner() {

		wawo::thread::UniqueLock<Mutex> _lg(m_mutex) ;
		if( !m_is_running ) {
			return wawo::E_TASK_RUNNER_POOL_INVALID_STATE ;
		}

		int size = m_runners.size() ;
		TaskRunner* tr = new TaskRunner(size) ;
		WAWO_RETURN_V_IF_MATCH( wawo::E_MEMORY_ALLOC_FAILED , tr == NULL );

		int rt = tr->Start();
		if( rt != wawo::OK ) {
			tr->Stop();
			WAWO_DELETE( tr );
			return rt;
		}
		m_runners.push_back( tr );
		return rt;
	}

	void TaskRunnerPool::SetMaxTaskRunner( u32_t const& count ) {
		UniqueLock<Mutex> _lg(m_mutex) ;
		WAWO_ASSERT( count>0 && count<WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT );

		m_max_concurrency = count;

		//dynamic clear ,,if ...
		while( m_runners.size() > m_max_concurrency ) {
			TaskRunner* runner = m_runners.back();
			WAWO_DELETE( runner ) ;
			m_runners.pop_back() ;
		}
	}

	//for sequence task
	SequencialTaskRunnerPool::SequencialTaskRunnerPool( u32_t const& concurrency ) :
		m_mutex(),
		m_is_running(false),
		m_concurrency(concurrency),
		m_runners()
	{
	}

	SequencialTaskRunnerPool::~SequencialTaskRunnerPool() {
		Deinit();
	}

	void SequencialTaskRunnerPool::Init() {
		m_is_running = true;
		WAWO_ASSERT( m_runners.size() == 0 );
		u32_t i = 0 ;
		while( i++ < m_concurrency ) {
			SequencialTaskRunner* r = new SequencialTaskRunner(i);
			WAWO_NULL_POINT_CHECK( r );
			int launch_rt = r->Start();
			WAWO_CONDITION_CHECK( launch_rt == wawo::OK );
			m_runners.push_back(r);
		}
		WAWO_ASSERT( m_runners.size() == m_concurrency );
	}

	void SequencialTaskRunnerPool::Deinit() {
		m_is_running = false;
		while( m_runners.size() ) {
			SequencialTaskRunner* r = m_runners.back();
			WAWO_DELETE( r ) ;
			m_runners.pop_back() ;
		}
		WAWO_ASSERT( m_runners.size() == 0 );
	}
}}