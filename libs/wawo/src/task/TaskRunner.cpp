#include <wawo/task/TaskRunner.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/task/TaskManager.h>

namespace wawo { namespace task {

	TaskRunner::TaskRunner( uint32_t const& runner_id ) :
		m_runner_id( runner_id ),
		m_state(TR_S_IDLE),
#ifdef USE_TASK_BUFFER
		m_task_current_assign_index(0),
		m_task_current_exec_index(-1),
		m_task_next_exec_index(0),
#else
		m_task(NULL),
#endif
		m_mutex(),
		m_condition()
	{

#ifdef USE_TASK_BUFFER
		for(uint32_t i=0;i<sizeof(m_task)/sizeof(m_task[0]);i++ ) {
			m_task[i] = NULL;
		}
#endif
		WAWO_LOG_DEBUG( "TRunner", "[-%d-]construct new TaskRunner", m_runner_id );
	}

	TaskRunner::~TaskRunner() {
		WAWO_LOG_DEBUG( "TRunner", "[-%d-]destructTaskRunner", m_runner_id );
		Stop() ;
	}

	void TaskRunner::OnStart() {
#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> _lg( m_mutex ) ;
#else
		LockGuard<Mutex> _lg( m_mutex ) ;
#endif

#ifdef USE_TASK_BUFFER
		m_state = TR_S_RUNNING ;
#else
		m_state = TR_S_WAITING ;
#endif
	}

	void TaskRunner::OnStop() {
#ifdef USE_TASK_BUFFER
		for(uint32_t i=0;i<sizeof(m_task)/sizeof(m_task[0]);i++ ) {
			WAWO_ASSERT( m_task[i] == NULL || !m_task[i]->Isset() );
		}
#endif
		WAWO_LOG_WARN("TRunner", "[-%d-]TaskRunner exit...", m_runner_id ) ;
	}

	void TaskRunner::Stop() {
		{

		#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg( m_mutex ) ;
		#else
			LockGuard<Mutex> _lg( m_mutex ) ;
		#endif

			if( m_state == TR_S_WAITING ) {
				m_condition.notify_one() ;
			} else if( m_state == TR_S_ASSIGNED ) {

#ifdef USE_TASK_BUFFER
				WAWO_ASSERT(!"ERROR STATE");
#else
				WAWO_ASSERT( m_task != WAWO_REF_PTR<Task_Abstract>::NULL_PTR );
				WAWO_ASSERT( m_task->Isset() );

				m_task->Cancel();
				m_task->Reset();
#endif
				m_condition.notify_one();//notify for exit
			} else if( m_state == TR_S_RUNNING ) {
#ifdef USE_TASK_BUFFER
				//we just wait m_task[m_task_current_exec_index] until it finish its execution
#else
				//waiting for it reach state of
				WAWO_ASSERT( "wawo thread core issue!!" );
#endif
			} else {}

			_CancelAll();
			m_state = TR_S_ENDING;
			WAWO_LOG_WARN("TRunner", "[-%d-]TaskRunner enter into state [%d]", m_runner_id, m_state ) ;
		}
		ThreadRunObject_Abstract::Stop();
	}

	uint32_t TaskRunner::CancelAll() {
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> lg(m_mutex);
	#else
		UniqueLock<Mutex> _ulk( m_mutex );
	#endif
		return _CancelAll();
	}
	uint32_t TaskRunner::_CancelAll() {

		uint32_t total_cancelled = 0;

#ifdef USE_TASK_BUFFER
		
		while( m_task_next_exec_index != m_task_current_assign_index ) {
			WAWO_ASSERT( m_task_next_exec_index >=0 && m_task_next_exec_index<TASK_BUFFER_SIZE );
			WAWO_ASSERT( m_task[m_task_next_exec_index]->Isset() );

			m_task[m_task_next_exec_index]->Cancel();
			m_task[m_task_next_exec_index]->Reset();
			++total_cancelled ;
			m_task_next_exec_index = ((m_task_next_exec_index+1))%TASK_BUFFER_SIZE;
		}
#else
		if( (m_task != WAWO_REF_PTR<Task_Abstract>::NULL_PTR) && m_task->Isset() ) {
			m_task->Cancel();
			m_task->Reset();
			++total_cancelled ;
		}
#endif
		WAWO_TRACK_TASK("TRunner", "[-%d-] cancel task: %d, tid: %d", total_cancelled, task->GetId() );
		return total_cancelled;
	}

	void TaskRunner::Run() {

#ifdef USE_TASK_BUFFER
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _ulk( m_mutex );
	#endif
			switch( m_state ) {
			case TR_S_RUNNING:
				{
					while( m_task_next_exec_index == m_task_current_assign_index ) {
						m_task_current_exec_index = -1; //no running task
						m_state = TR_S_WAITING;
						#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
						m_condition.wait<SpinMutex>(m_mutex);
						#else
						m_condition.wait(_ulk);
						#endif
						//edge check
						if( m_state == TR_S_ENDING ) {
							return ;
						}
						WAWO_ASSERT( m_state == TR_S_WAITING );
						m_state = TR_S_RUNNING;
					}
					WAWO_ASSERT( (((m_task_current_assign_index - m_task_next_exec_index))%TASK_BUFFER_SIZE) != 0 );
					m_task_current_exec_index = m_task_next_exec_index;
					m_task_next_exec_index = ((m_task_next_exec_index + 1))%TASK_BUFFER_SIZE;
				}
				break;
			case TR_S_ENDING:
				{
					return ;
				}
			case TR_S_WAITING:
			case TR_S_IDLE:
				{
					WAWO_ASSERT( !"invalid state");
				}
			default:
				{
					WAWO_THROW_EXCEPTION("task runner state exception");
				}
			}
		}

		WAWO_ASSERT( m_task_current_exec_index >=0 && m_task_current_exec_index<TASK_BUFFER_SIZE );
		WAWO_ASSERT( m_task[m_task_current_exec_index]->Isset() );

		WAWO_TRACK_TASK("TRunner", "[-%d-]task run begin, tid: %d",m_runner_id, m_task[_run_idx]->GetId() );
		try {
			m_task[m_task_current_exec_index]->Run();
		} catch ( wawo::Exception& e ) {
			m_task[m_task_current_exec_index]->Reset();

			#ifdef _DEBUG
			WAWO_THROW(e);
			#else
			WAWO_LOG_FATAL("TRunner", "[-%d-]task_runner wawo::Exception: %s\n%s[%d] %s\n%s", m_runner_id, e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace()  );
			#endif
		} catch( std::exception& e) {
			m_task[m_task_current_exec_index]->Reset();

			#ifdef _DEBUG
			WAWO_THROW(e);
			#else
			WAWO_LOG_FATAL("TRunner", "[-%d-]task_runner exception: %s", m_runner_id, e.what() );
			#endif
		} catch( ... ) {
			m_task[m_task_current_exec_index]->Reset();
			WAWO_THROW_EXCEPTION("unknown exception in task runner !!!!");
		}

		WAWO_TRACK_TASK("TRunner", "[-%d-]task run end, tid: %d",m_runner_id, m_task[_run_idx]->GetId() );

		m_task[m_task_current_exec_index]->Reset();
#else
		UniqueLock<Mutex> _ulk( m_mutex ) ;
		if( m_state == TR_S_ENDING ) {
			//for edge switch
			return ;
		}

		if( m_task == WAWO_REF_PTR<Task_Abstract>::NULL_PTR || !m_task->Isset() ) {
			m_state = TR_S_WAITING ;
	#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
				m_condition.Wait(_ulk.Mutex());
	#else
				m_condition.Wait(_ulk);
	#endif
		}

		if( m_state == TR_S_ENDING ) {
			WAWO_ASSERT( m_task == WAWO_REF_PTR<Task_Abstract>::NULL_PTR || !m_task->Isset() );
			//for waiting wakeup
			return ;
		}

		WAWO_ASSERT( m_task->Isset() );
		WAWO_ASSERT( m_state == TR_S_ASSIGNED );
		m_state = TR_S_RUNNING;

		try {
			m_task->Run();
		} catch ( std::exception& e ) {

	#ifdef _DEBUG
			WAWO_THROW_EXCEPTION(e);
	#else
			WAWO_LOG_FATAL("task_runner", "[%d]task_runner exception: %s", m_runner_id, e.what() );
	#endif
		}

		m_task->Reset();
		m_state = TR_S_WAITING ;
#endif
	}

	SequenceTaskRunner::SequenceTaskRunner(uint32_t const& sequence):
		m_state(TR_S_IDLE),
		m_sequence(sequence)
	{
		m_executing = new SequenceTaskVector() ;
		WAWO_NULL_POINT_CHECK( m_executing );

		m_to_executing = new SequenceTaskVector();
		WAWO_NULL_POINT_CHECK( m_to_executing ) ;
	}

	SequenceTaskRunner::~SequenceTaskRunner() {
		Stop();
		WAWO_DELETE( m_executing );
		WAWO_DELETE( m_to_executing );
	}

	int SequenceTaskRunner::AssignTask( WAWO_REF_PTR<SequenceTask> const& task ) {
		#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> _lg( m_mutex ) ;
		#else
			LockGuard<Mutex> _lg( m_mutex ) ;
		#endif

		if( m_state != TR_S_RUNNING ) {
			return wawo::E_TASK_MANAGER_INVALID_STATE ;
		}

		m_to_executing->push_back( task );

		return wawo::OK ;
	}
	void SequenceTaskRunner::OnStart() {
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _lg( m_mutex );
	#endif
			m_state = TR_S_RUNNING;
	}

	void SequenceTaskRunner::Stop() {
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _lg( m_mutex );
	#endif
			if( m_state == TR_S_WAITING ) {
				m_condition.notify_one();
			}

			m_state = TR_S_ENDING ;
			m_to_executing->clear();
		}

		ThreadRunObject_Abstract::Stop();
	}

	void SequenceTaskRunner::OnStop() {

		WAWO_ASSERT( m_to_executing->empty() );
		WAWO_ASSERT( m_executing->empty() );

		//m_to_executing->clear();
		//m_executing->clear();
	}

	void SequenceTaskRunner::Run() {
		//check state,
		//check list
		//check state
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _ulk( m_mutex );
	#endif
			if( m_state == TR_S_ENDING ) {
				//exit
				return ;
			}

			if( m_to_executing->empty() ) {
				m_state = TR_S_WAITING ;
#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
				m_condition.wait<SpinMutex>(m_mutex);
#else
				m_condition.wait( _ulk );
#endif
			}

			if( m_state == TR_S_ENDING ) {
				//double check for wakeup
				return ;
			}

			m_state = TR_S_RUNNING;
			WAWO_ASSERT( m_executing->empty() );
			WAWO_ASSERT( !m_to_executing->empty() );
			wawo::swap( m_to_executing, m_executing );
			WAWO_ASSERT( !m_executing->empty() );
			WAWO_ASSERT( m_to_executing->empty() );
		}

		int i = 0;
		int size = m_executing->size();

		while( m_state == TR_S_RUNNING && i++ != size ) {
			WAWO_REF_PTR<SequenceTask>& task = (*m_executing)[i] ;
			WAWO_ASSERT( task != NULL && task->Isset() );

			try {
				task->Run();
			} catch ( std::exception& e ) {
				WAWO_LOG_FATAL("task_runner", "[%d] SequenceTaskRunner exception: %s", m_sequence, e.what() );
			}
			task->Reset();
		}

	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _ulk( m_mutex );
	#endif
			m_executing->clear();
	}
}}//END OF NS
