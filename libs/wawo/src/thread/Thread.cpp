#include <thread>

#include <wawo/core.h>
#include <wawo/thread/Thread.h>
#include <wawo/log/LoggerManager.h>

namespace wawo { namespace thread {

	Thread::Thread( ThreadObject_Abstract* thread_object, const char* name ) :
#ifdef _DEBUG_THREAD
		m_state(TS_IDLE),
#endif
		m_threadHandler(NULL)
	{
		WAWO_ASSERT( thread_object != NULL );
		m_threadObject = thread_object ;

		if(name && strlen(name) )
		{
			m_threadName = (char*) malloc( sizeof(char)*( strlen(name) +1) ) ;
			WAWO_NULL_POINT_CHECK( m_threadName );
			wawo::strcpy(m_threadName, name);
			m_threadName[strlen(name)] = 0;
		}
	}

	Thread::~Thread() {
		Stop();

		if(m_threadName) {
			free ( m_threadName ) ;
			m_threadName = 0 ;
		}
	}

	int Thread::Start() {

#ifdef _DEBUG_THREAD
		WAWO_ASSERT( m_state == TS_IDLE || m_state == TS_ENDING );
#endif
		WAWO_ASSERT( m_threadHandler == NULL );

		std::thread* tptr = NULL;
		try {
			tptr = new std::thread( &ThreadObject_Abstract::__RUN__, m_threadObject ) ;
		} catch( std::exception& e) {
			WAWO_DELETE( tptr ) ;
			WAWO_LOG_FATAL("thread", "new std::thread(&ThreadObject_Abstract::__RUN__, m_threadObject) failed: %d:%s", e.what() );
			return wawo::E_LAUNCH_STD_THREAD_FAILED;
		}

		m_threadHandler = (void*) tptr;
		WAWO_RETURN_V_IF_MATCH( wawo::E_MEMORY_ALLOC_FAILED, (m_threadHandler == NULL) ) ;

#ifdef _DEBUG_THREAD
		m_state = TS_RUNNING;
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
		wawo::thread::ThreadDebugger::GetInstance()->IncreaseCreate();
#endif
		WAWO_LOG_DEBUG( "THREAD", "new thread(%s) started", m_threadName ) ;
		return wawo::OK ;
	}

	void Thread::Interrupt() {

		WAWO_ASSERT( m_threadHandler != NULL );

#ifdef _DEBUG_THREAD
		WAWO_ASSERT( m_state == TS_RUNNING );
		m_state = TS_INTERRUPTED ;
#endif
		WAWO_THROW_EXCEPTION( "TO DO" ) ;
	}


	void Thread::Join() {

		if( Joinable() ) {

#ifdef _DEBUG_THREAD
		WAWO_ASSERT( m_threadHandler != NULL ) ;
		WAWO_ASSERT( m_state == TS_RUNNING ) ;
#endif
			WAWO_ASSERT( m_threadHandler != NULL ) ;
			std::thread* thread = static_cast< std::thread* >( m_threadHandler );
			thread->join();

#ifdef _DEBUG_THREAD
			m_state = TS_ENDING ;
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread::ThreadDebugger::GetInstance()->IncreaseJoin();
#endif
			WAWO_LOG_DEBUG( "THREAD", "new thread(%s) ended", m_threadName );
		}

	}

	bool Thread::Joinable() {
		if( m_threadHandler == NULL ) { return false; }
		WAWO_ASSERT( m_threadHandler != NULL ) ;
		std::thread* thread = static_cast< std::thread* >( m_threadHandler );
		return thread->joinable() ;
	}

	void Thread::Stop() {
		Join();

		if( m_threadHandler != NULL ) {
			std::thread* thread = static_cast< std::thread* >( m_threadHandler );
			WAWO_DELETE( thread );
			m_threadHandler = NULL;
		}
	}
}}