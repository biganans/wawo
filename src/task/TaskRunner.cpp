#include <wawo/log/LoggerManager.h>
#include <wawo/task/TaskManager.hpp>
#include <wawo/task/TaskRunner.hpp>

namespace wawo { namespace task {

	TaskRunner::TaskRunner( u32_t const& id ) :
		m_id( id ),
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
		for(u32_t i=0;i<sizeof(m_task)/sizeof(m_task[0]);i++ ) {
			m_task[i] = NULL;
		}
#endif
		WAWO_DEBUG( "[TRunner][-%d-]construct new TaskRunner", m_id );
	}

	TaskRunner::~TaskRunner() {
		Stop() ;
		WAWO_DEBUG( "[TRunner][-%d-]destructTaskRunner", m_id );
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
		m_state = TR_S_IDLE ;
#endif
	}

	void TaskRunner::OnStop() {
#ifdef USE_TASK_BUFFER
		for(u32_t i=0;i<sizeof(m_task)/sizeof(m_task[0]);i++ ) {
			WAWO_ASSERT( m_task[i] == NULL || !m_task[i]->Isset() );
		}
#else
		WAWO_ASSERT( m_task == NULL || !m_task->Isset() );
#endif
		WAWO_DEBUG("[TRunner][-%d-]TaskRunner exit...", m_id ) ;
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
			}

			_Cancel();
			m_state = TR_S_ENDING;
			WAWO_DEBUG("[TRunner][-%d-]TaskRunner enter into state [%d]", m_id, m_state ) ;
		}
		ThreadRunObject_Abstract::Stop();
	}

	u32_t TaskRunner::Cancel() {
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> lg(m_mutex);
	#else
		LockGuard<Mutex> lg( m_mutex );
	#endif
		return _Cancel();
	}

	u32_t TaskRunner::_Cancel() {
#ifdef USE_TASK_BUFFER
		u32_t total_cancelled = 0;
		while( m_task_next_exec_index != m_task_current_assign_index ) {
			WAWO_ASSERT( m_task_next_exec_index >=0 && m_task_next_exec_index<TASK_BUFFER_SIZE );
			WAWO_ASSERT( m_task[m_task_next_exec_index]->Isset() );

			m_task[m_task_next_exec_index]->Cancel();
			m_task[m_task_next_exec_index]->Reset();
			++total_cancelled ;
			m_task_next_exec_index = ((m_task_next_exec_index+1))%TASK_BUFFER_SIZE;
		}
		WAWO_TRACK_TASK("[TRunner][-%d-] cancel task, TID: %llu", m_runner_id, reinterpret_cast<u64_t>( task.Get() ) );
		return total_cancelled;
#else
		if( m_task != NULL && m_task->Isset() ) {
			m_task->Cancel();
			m_task->Reset();
			return 1;
		}
		return 0;
#endif
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

		WAWO_TRACK_TASK("[TRunner][-%d-]task run begin, TID: %llu", m_id, reinterpret_cast<u64_t>( m_task[m_task_current_exec_index].Get() ) );
		try {
			m_task[m_task_current_exec_index]->Run();
		} catch ( wawo::Exception& e ) {
			m_task[m_task_current_exec_index]->Reset();

			#ifdef _DEBUG
			WAWO_THROW(e);
			#else
			WAWO_FATAL("[TRunner][-%d-]task_runner wawo::Exception: %s\n%s[%d] %s\n%s", m_id, e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace()  );
			#endif
		} catch( std::exception& e) {
			m_task[m_task_current_exec_index]->Reset();

			#ifdef _DEBUG
			WAWO_THROW(e);
			#else
			WAWO_FATAL("[TRunner][-%d-]task_runner exception: %s", m_id, e.what());
			#endif
		} catch( ... ) {
			m_task[m_task_current_exec_index]->Reset();
			WAWO_THROW_EXCEPTION("unknown exception in task runner !!!!");
		}

		WAWO_TRACK_TASK("[TRunner][-%d-]task run end, TID: %llu", m_id, reinterpret_cast<u64_t>( m_task[m_task_current_exec_index].Get() ) );
		m_task[m_task_current_exec_index]->Reset();
#else
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _ulk( m_mutex );
	#endif

			while( m_state != TR_S_ENDING )
			{
				if( m_task == NULL || !m_task->Isset() ) {
					m_state = TR_S_WAITING ;
					#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
						m_condition.wait<SpinMutex>(m_mutex);
					#else
						m_condition.wait(_ulk);
					#endif

					switch( m_state )
					{
					case TR_S_ASSIGNED:
						{
							WAWO_ASSERT( m_task->Isset() );
							m_state = TR_S_RUNNING;
							try {
								WAWO_TRACK_TASK("[TRunner][-%d-]task run begin, TID: %llu", m_id, reinterpret_cast<u64_t>( m_task.Get() ) );
								m_task->Run();
								WAWO_TRACK_TASK("[TRunner][-%d-]task run end, TID: %llu", m_id, reinterpret_cast<u64_t>( m_task.Get() ) );
								m_task->Reset();
							} catch ( wawo::Exception& e ) {
								m_task->Reset();
								WAWO_FATAL("[TRunner][-%d-]task_runner wawo::Exception: %s\n%s[%d] %s\n%s", m_id, e.GetInfo(), e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace());
								WAWO_THROW(e);
							} catch( std::exception& e) {
								m_task->Reset();
								WAWO_FATAL("[TRunner][-%d-]task_runner exception: %s", m_id, e.what());
								WAWO_THROW(e);
							} catch( ... ) {
								m_task->Reset();
								WAWO_THROW_EXCEPTION("unknown exception in task runner !!!!");
							}
						}
						break;
						case TR_S_WAITING:
						case TR_S_IDLE:
						case TR_S_ENDING:
							break;
						case TR_S_RUNNING:
						{
							WAWO_THROW_EXCEPTION("wawo task runner logic issue");
						}
						break;
					}
				}
			}
		}
#endif
	}


#define WAWO_SQT_VECTOR_SIZE 4000

	SequencialTaskRunner::SequencialTaskRunner(u32_t const& id):
		m_mutex(),
		m_condition(),
		m_state(S_IDLE),
		m_id(id),
		m_standby(NULL),
		m_assigning(NULL),
		m_notify(true),
		m_assign_cancel_c(0)
	{
	}

	SequencialTaskRunner::~SequencialTaskRunner() {
		Stop();
	}

	void SequencialTaskRunner::OnStart() {
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			LockGuard<Mutex> _lg( m_mutex );
	#endif
			m_state = S_RUN;

			m_standby = new SequencialTaskVector() ;
			WAWO_NULL_POINT_CHECK( m_standby );

			m_assigning = new SequencialTaskVector();
			WAWO_NULL_POINT_CHECK( m_assigning ) ;

			m_standby->reserve( WAWO_SQT_VECTOR_SIZE );
			m_assigning->reserve( WAWO_SQT_VECTOR_SIZE );
	}

	void SequencialTaskRunner::Stop() {
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			LockGuard<Mutex> _lg( m_mutex );
	#endif
			if( m_state == S_EXIT ) return ;
			m_state = S_EXIT ;
			_CancelAll();
			m_condition.notify_one();
		}

		ThreadRunObject_Abstract::Stop();
	}

	void SequencialTaskRunner::OnStop() {
		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_ASSERT( m_standby->empty() );
		WAWO_ASSERT( m_assigning->empty() );
		WAWO_DELETE( m_standby );
		WAWO_DELETE( m_assigning );
	}

	u32_t SequencialTaskRunner::CancelAll() {
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
		LockGuard<SpinMutex> lg(m_mutex);
	#else
		LockGuard<Mutex> _lg( m_mutex );
	#endif
		return _CancelAll();
	}

	u32_t SequencialTaskRunner::_CancelAll() {

		State previous_state = m_state;
		m_state = S_CANCELLING;

		u32_t total = 0;

		std::for_each( m_standby->begin(), m_standby->end(), [&total](WWRP<SequencialTask> const& t) {
			t->Cancel();
			t->Reset();
			++total;
		});
		m_standby->clear();

		while( m_assigning->size() );
		total += m_assign_cancel_c;
		m_assign_cancel_c = 0;

		m_state = previous_state;

		return total;
	}


	void SequencialTaskRunner::Run() {
		{
	#ifdef	WAWO_TASK_RUNNER_USE_SPIN_MUTEX
			LockGuard<SpinMutex> lg(m_mutex);
	#else
			UniqueLock<Mutex> _ulk( m_mutex );
	#endif
			if( m_state == S_EXIT ) {
				return ;
			}

			while( m_standby->empty() ) {
				m_notify = true;
#ifdef WAWO_TASK_RUNNER_USE_SPIN_MUTEX
				m_condition.wait<SpinMutex>(m_mutex);
#else
				m_condition.wait( _ulk );
#endif
				m_notify = false;
				if( m_state == S_EXIT ) {
					return ;
				}

				WAWO_ASSERT( m_state == S_RUN );
			}

			WAWO_ASSERT( m_assigning->empty() );
			WAWO_ASSERT( !m_standby->empty() );
			wawo::swap( m_standby, m_assigning );
			WAWO_ASSERT( !m_assigning->empty() );
			WAWO_ASSERT( m_standby->empty() );
		}

		int i = 0;
		int size = m_assigning->size();
		while( m_state == S_RUN && i != size ) {
			WWRP<SequencialTask>& task = (*m_assigning)[i++] ;
			WAWO_ASSERT( task != NULL && task->Isset() );
			try {
				task->Run();
				task->Reset();
			} catch ( wawo::Exception& e ) {
				task->Reset();
				WAWO_FATAL("[SQTRunner][-%d-]wawo::Exception: %s\n%s[%d] %s\n%s", m_id, e.GetInfo(), e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace());
				WAWO_THROW(e);
			} catch( std::exception& e) {
				task->Reset();
				WAWO_FATAL("[SQTRunner][-%d-]exception: %s", m_id, e.what());
				WAWO_THROW(e);
			} catch( ... ) {
				task->Reset();
				WAWO_THROW_EXCEPTION("SQTRunner -- unknown exception in task runner !!!!");
			}
		}

		if( (m_state != S_RUN) && i != size ) {
			for(int j=i;j<size;j++) {
				(*m_assigning)[j]->Cancel();
				(*m_assigning)[j]->Reset();
			}
			m_assign_cancel_c = size-i;
			WAWO_DEBUG("[SQTRunner][-%d-]cancelled: %d, for reason not in S_RUN state", m_id, (size-i) );
		}

		if( (m_state == S_RUN) && (size>WAWO_SQT_VECTOR_SIZE) ) {
			m_assigning->resize(WAWO_SQT_VECTOR_SIZE);
		}
		m_assigning->clear();
	}

}}//END OF NS
