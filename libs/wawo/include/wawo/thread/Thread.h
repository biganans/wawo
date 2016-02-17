#ifndef _WAWO_THREAD_THREAD_H_
#define _WAWO_THREAD_THREAD_H_

#include <thread>
#include <exception>

#include <wawo/core.h>

#ifdef _DEBUG_THREAD
	#include <wawo/thread/Tls.hpp>
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
	#include <atomic>
#endif

#include <wawo/Exception.hpp>
#include <wawo/log/LoggerManager.h>


namespace wawo { namespace thread {

#define _DEBUG_THREAD_CREATE_JOIN
#ifdef _DEBUG_THREAD_CREATE_JOIN
	struct ThreadDebugger: public wawo::SafeSingleton<ThreadDebugger> {
		std::atomic<uint32_t> m_created;
		std::atomic<uint32_t> m_joined;

		ThreadDebugger():
			m_created(0),
			m_joined(0){
		}
		~ThreadDebugger() {
			WAWO_CONDITION_CHECK( m_created == m_joined );
		}

		void IncreaseCreate() {
			m_created++;
		}
		void IncreaseJoin() {
			m_joined++;
		}
	};
#endif

	class ThreadObject_Abstract;
	class Thread:
		public wawo::NonCopyable
	{

	public:
		enum ThreadError {
			ERROR_THREAD_ALREADY_STARTED = -1
		};

		enum Priority {
			P_MAXIMU,
			P_HIGH,
			P_NORMAL,
			P_LOW,
			P_MINIMUM
		};

		enum State {
			TS_IDLE,
			TS_RUNNING,
			TS_INTERRUPTED,
			TS_ENDING,
			TS_ERROR,
		};

		Thread( ThreadObject_Abstract* const run_object, char const* name = 0 );
		virtual ~Thread();

		int Start();
		virtual void Stop();
		void Interrupt(); //not implemented

		void Join();
		bool Joinable();

#ifdef _DEBUG_THREAD
		inline Thread::State const& GetState() { return m_state ; }
		inline Thread::State const& GetState() const { return m_state ; }
#endif

	private:
		ThreadObject_Abstract* m_threadObject;

#ifdef _DEBUG_THREAD
		State m_state;
#endif
		void* m_threadHandler ;
		char* m_threadName;
	};

	//will run for once
	class ThreadObject_Abstract:
		virtual public wawo::NonCopyable
	{
		friend int Thread::Start();
	private:
		Thread* m_thread;
	public:
		ThreadObject_Abstract() :
			m_thread(NULL)
		{
			m_thread = new Thread( this, "ThreadObject" );
			WAWO_NULL_POINT_CHECK( m_thread )
		}

		virtual ~ThreadObject_Abstract() {
			WAWO_ASSERT( m_thread );

			StopThread();
			WAWO_DELETE( m_thread );
		}

		virtual void operator() () = 0 ;

		inline Thread* GetThread() {
			WAWO_ASSERT( m_thread != NULL );
			return m_thread ;
		}

		inline int StartThread(){
			WAWO_ASSERT( m_thread != NULL );
			return m_thread->Start();
		}

		inline void InterruptThread() {
			WAWO_ASSERT( m_thread );
			m_thread->Interrupt() ;
		}

		inline int JoinThread() {
			WAWO_ASSERT( m_thread );

			if(m_thread->Joinable()) {
				m_thread->Join();
			}

			return wawo::OK ;
		}

		virtual void __OnStart__() {} ;
		virtual void __OnStop__() {} ;

		void StopThread() {
			WAWO_ASSERT( m_thread != NULL );
			m_thread->Stop();
		}
	private:
		inline void __OnLaunching__() {
			#ifdef WAWO_PLATFORM_POSIX
			//check stack size
			size_t ssize = wawo::get_stack_size();
			WAWO_LOG_DEBUG("ThreadObject_Abstract::__OnLaunching__()", "thread stack size: %d", ssize );
			WAWO_ASSERT( ssize >= (1024*1024*4) ) ; //4M
			#endif
		}
		inline void __OnExit__() {

#ifdef _DEBUG_MUTEX
			WAWO_TLS_RELEASE(std::set<ptrdiff_t>);
#endif
		}

		inline void __RUN__() {
			WAWO_ASSERT( m_thread != NULL );

			try {
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnLaunching__() begin" );
				__OnLaunching__();
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnLaunching__() begin" );
			} catch (wawo::Exception& e) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnLaunching__", "wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnLaunching__", "std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnLaunching__ ", "unknown thread exception" );
				WAWO_THROW_EXCEPTION("unknown thread exception");
			}

			try {
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnStart__() begin" );
				__OnStart__();
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnStart__() begin" );
			} catch (wawo::Exception& e) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStart__", "wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStart__", "std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStart__ ", "unknown thread exception" );
				WAWO_THROW_EXCEPTION("unknown thread exception");
			}

			try {
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread operator() begin" );
				operator()() ;
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread operator() begin" );
			} catch (wawo::Exception& e) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::operator()", "wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::operator()", "std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::operator() ", "unknown thread exception" );
				WAWO_THROW_EXCEPTION("unknown thread exception");
			}

			try {
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnStop__() begin" );
				__OnStop__();
				WAWO_LOG_DEBUG("ThreadObject_Abstract::__RUN()__", "thread __OnStop__() end" );
			} catch (wawo::Exception& e) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStop__", "wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStop__", "thread __OnStop__() exception : %s", e.what() ) ;
				WAWO_THROW(e); //the caller have to check their exceptions
			} catch( ... ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnStop__", "unknown thread exception" );
				WAWO_THROW_EXCEPTION("unknown thread exception");
			}

			try {
				__OnExit__();
			} catch (wawo::Exception& e) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnExit__", "wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnExit__", "thread __OnStop__() exception : %s", e.what() ) ;
				WAWO_THROW(e); //the caller have to check their exceptions
			} catch(...) {
				WAWO_LOG_FATAL("ThreadObject_Abstract::__OnExit__", "unknown thread exception" );
				WAWO_THROW_EXCEPTION("unknown thread exception");
			}
		}
	};

	//will run until you call stop
	class ThreadRunObject_Abstract:
		public ThreadObject_Abstract {

	private:
		enum ThreadRunObjectState {
			TR_IDLE,
			TR_START,
			TR_RUNNING,
			TR_ERROR,
			TR_STOP,
		};

	public:
		ThreadRunObject_Abstract() :
			ThreadObject_Abstract(),
			m_state(TR_IDLE)
		{
		}

		virtual ~ThreadRunObject_Abstract() {
			Stop();
		}

	public:
		inline ThreadRunObjectState GetState() const {
			return m_state;
		}

		inline bool IsThreadStateRunning() const {
			return m_state == TR_RUNNING ;
		}

		inline bool IsThreadStateIdleing() const {
			return m_state == TR_IDLE;
		}

		virtual void __OnStart__() {
			WAWO_ASSERT( m_state == TR_IDLE );
			m_state = TR_START ;
			OnStart();
		}
		virtual void __OnStop__() {
			WAWO_ASSERT( m_state == TR_STOP );
			OnStop();
		}

		virtual void OnStart() = 0 ;
		virtual void OnStop() = 0 ;
		virtual void Run() = 0;

		int Start( bool const& block_start = true ) {

			WAWO_ASSERT( m_state == TR_IDLE );

			if( block_start ) {
				int start_thread = StartThread();
				if( start_thread != wawo::OK ) {
					return start_thread;
				}
				for(int k=0; !IsThreadStateRunning() ;k++ ) {
					wawo::yield(k);
				}
				return wawo::OK ;
			} else {
				return StartThread() ;
			}
		}

		void Stop() {

			WAWO_ASSERT( (m_state == TR_RUNNING || m_state == TR_IDLE) ) ;

			if( m_state == TR_RUNNING ) {
				m_state = TR_STOP ;
			}

			//for none block start,
			StopThread() ;
			m_state = TR_IDLE;
		}

	private:
		ThreadRunObjectState m_state;
	public:
		void operator()() {
			WAWO_ASSERT( m_state == TR_START );
			m_state = TR_RUNNING ; //if launch failed, it will not in TR_RUNNING ..., so don't worry
			bool e = false;
			do {
				switch( m_state ) {
				case TR_RUNNING:
					{
						Run();
					}
					break;
				case TR_STOP:
				case TR_ERROR:
					{
						e = true;
					}
					break;
				case TR_START:
				case TR_IDLE:
				default:
					{
						WAWO_THROW_EXCEPTION("invalid thread state");
					}
					break;
				}
			} while(!e);
		}
	};
}}
#endif
