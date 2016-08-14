#include <thread>

#include <wawo/core.h>
#include <wawo/thread/Thread.hpp>
#include <wawo/log/LoggerManager.h>

namespace wawo { namespace thread {

	Thread::Thread( ThreadObject_Abstract* object, Len_CStr const& name ) :
		m_object(object),
		m_handler(NULL),
		m_state(TS_IDLE),
		m_name(name)
	{
		WAWO_ASSERT( m_object != NULL );
	}

	Thread::~Thread() {
		Stop();
	}

	int Thread::Start() {

		WAWO_ASSERT( m_state == TS_IDLE || m_state == TS_ENDING );
		WAWO_ASSERT( m_handler == NULL );
		WAWO_ASSERT( m_object != NULL );

		std::thread* _THH_ = NULL;
		try {
			_THH_ = new std::thread( &ThreadObject_Abstract::__RUN__, m_object ) ;
		} catch( ... ) {
			WAWO_DELETE(_THH_) ;
			int _eno = wawo::GetLastErrno();
			WAWO_FATAL("[thread]new std::thread(&ThreadObject_Abstract::__RUN__, m_threadObject) failed: %d", _eno);
			return WAWO_NEGATIVE(_eno);
		}

		WAWO_RETURN_V_IF_MATCH( wawo::E_MEMORY_ALLOC_FAILED, (_THH_ == NULL) ) ;
		m_handler = (void*)_THH_;
		m_state = TS_LAUNCHED;

#ifdef _DEBUG_THREAD_CREATE_JOIN
		wawo::thread::ThreadDebugger::GetInstance()->IncreaseCreate();
#endif
		WAWO_DEBUG( "[THREAD]wawo::thread(%s) started", m_name.CStr() ) ;
		return wawo::OK ;
	}

	void Thread::Interrupt() {
		WAWO_THROW_EXCEPTION( "DO NOT SUPPORT RIGHT NOW" ) ;
		//WAWO_ASSERT( m_handler != NULL );
		//WAWO_ASSERT( m_state == TS_LAUNCHED );
		//m_state = TS_INTERRUPTED ;
	}

	void Thread::Join() {

		if( Joinable() ) {
			WAWO_ASSERT( m_handler != NULL ) ;
			WAWO_ASSERT( m_state == TS_LAUNCHED ) ;

			std::thread* _THH_ = static_cast<std::thread*>( m_handler );
			_THH_->join();
			m_state = TS_ENDING ;

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread::ThreadDebugger::GetInstance()->IncreaseJoin();
#endif
			WAWO_DEBUG( "[THREAD] wawo::thread(%s) ended", m_name.CStr() );
		}
	}

	bool Thread::Joinable() {
		if( m_handler == NULL ) { return false; }
		std::thread* thread = static_cast<std::thread*>( m_handler );
		return thread->joinable() ;
	}

	void Thread::Stop() {
		Join();

		if( m_handler != NULL ) {
			std::thread* _THH_ = static_cast<std::thread*>( m_handler );
			WAWO_DELETE(_THH_);
			m_handler = NULL;
		}
	}
}}