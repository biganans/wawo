#ifndef _WAWO_THREAD_THREAD_HPP_
#define _WAWO_THREAD_THREAD_HPP_

#include <thread>
#include <exception>

#ifdef _DEBUG
	#ifndef _DEBUG_THREAD
		#define _DEBUG_THREAD
	#endif
#endif

#ifdef _DEBUG_THREAD
	#define _DEBUG_THREAD_CREATE_JOIN
#endif

#ifdef _DEBUG_THREAD_CREATE_JOIN
	#include <atomic>
#endif

#include <wawo/core.h>
#include <wawo/thread/Mutex.hpp>
#include <wawo/Exception.hpp>
#include <wawo/log/LoggerManager.h>


namespace wawo { namespace thread {

#ifdef _DEBUG_THREAD_CREATE_JOIN
	struct ThreadDebugger: public wawo::Singleton<ThreadDebugger> {
		std::atomic<u32_t> m_created;
		std::atomic<u32_t> m_joined;
		ThreadDebugger():
			m_created(0),
			m_joined(0)
		{
		}
		~ThreadDebugger() {}
		void Init() { m_created = 0; m_joined = 0; }
		void Deinit() { WAWO_CONDITION_CHECK(m_created == m_joined); }
		void IncreaseCreate() { ++m_created;}
		void IncreaseJoin() { ++m_joined;}
	};
#endif

	class ThreadObject_Abstract;
	class Thread final:
		public wawo::NonCopyable
	{

	public:
		enum Priority {
			P_MAXIMU,
			P_HIGH,
			P_NORMAL,
			P_LOW,
			P_MINIMUM
		};

		enum State {
			TS_IDLE,
			TS_LAUNCHED,
			TS_INTERRUPTED,
			TS_ENDING,
			TS_ERROR,
		};

		Thread( ThreadObject_Abstract* const object, Len_CStr const& name );
		~Thread();

		int Start();
		void Stop();
		void Interrupt(); //not implemented yet

		void Join();
		bool Joinable();

		inline Thread::State const& GetState() const { return m_state ; }
	private:
		ThreadObject_Abstract* m_object;
		void* m_handler ;
		State m_state;
		Len_CStr m_name;
	};

	class ThreadObject_Abstract:
		virtual public wawo::RefObject_Abstract
	{
		friend int Thread::Start();
	private:
		Thread* m_thread;
	public:
		ThreadObject_Abstract() :
			m_thread(NULL)
		{
		}

		virtual ~ThreadObject_Abstract() {
			StopThread();
		}

		int StartThread() {
			WAWO_ASSERT( m_thread == NULL );
			try {
				m_thread = new Thread( this, "ThreadObject" );
			} catch(...) {
				WAWO_DELETE(m_thread);
				int _eno = wawo::GetLastErrno();
				WAWO_FATAL("[ThreadObject_Abstract::StartThread] failed: %d", _eno );
				return _eno;
			}
			return m_thread->Start();
		}
		void StopThread() {
			if(m_thread != NULL) {
				m_thread->Stop();
				WAWO_DELETE(m_thread);
			}
		}
		void InterruptThread() {
			WAWO_ASSERT( m_thread != NULL );
			m_thread->Interrupt() ;
		}
		void JoinThread() {
			WAWO_ASSERT( m_thread != NULL );
			if(m_thread->Joinable()) {
				m_thread->Join();
			}
		}

		virtual void __OnStart__() {} ;
		virtual void __OnStop__() {} ;

		virtual void operator() () = 0 ;
	private:
		inline void __OnLaunching__() {
			#ifdef WAWO_PLATFORM_GNU
			//check stack size
			size_t ssize = wawo::get_stack_size();
			WAWO_DEBUG("[ThreadObject_Abstract::__OnLaunching__()]thread stack size: %d", ssize );
			WAWO_ASSERT( ssize >= (1024*1024*4) ) ; //4M
			(void)ssize;
			#endif
		}
		inline void __OnExit__() {
#ifdef _DEBUG_MUTEX
			WAWO_TLS_RELEASE(std::set<void*>);
#endif
		}

		inline void __RUN__() {
			WAWO_ASSERT( m_thread != NULL );
			try {
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnLaunching__() begin" );
				__OnLaunching__();
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnLaunching__() end" );
			} catch (wawo::Exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN__]__OnLaunching__() wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch(std::exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN__]__OnLaunching__() std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN__]__OnLaunching__() unknown thread exception" );
				WAWO_THROW_EXCEPTION("__OnLaunching__() unknown thread exception");
			}

			try {
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN__]__OnStart__() begin" );
				__OnStart__();
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN__]__OnStart__() end" );
			} catch (wawo::Exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN__]__OnStart__() wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch(std::exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN__]__OnStart__() std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_FATAL("[ThreadObject_Abstract::__OnStart__]__OnStart__() unknown thread exception" );
				WAWO_THROW_EXCEPTION("__OnStart__() unknown thread exception");
			}

			try {
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]thread operator() begin" );
				operator()() ;
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]thread operator() end" );
			} catch (wawo::Exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::operator()]operator() wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_FATAL("[ThreadObject_Abstract::operator()]operator() std::exception: %s", e.what() );
				WAWO_THROW(e);
				//the caller have to check their exceptions
			} catch( ... ) {
				WAWO_FATAL("[ThreadObject_Abstract::operator()]operator() unknown thread exception" );
				WAWO_THROW_EXCEPTION("operator() unknown thread exception");
			}

			try {
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnStop__() begin" );
				__OnStop__();
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnStop__() end" );
			} catch (wawo::Exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnStop__()  wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch(std::exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnStop__() std::exception : %s", e.what() ) ;
				WAWO_THROW(e); //the caller have to check their exceptions
			} catch( ... ) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnStop__() unknown thread exception" );
				WAWO_THROW_EXCEPTION("__OnStop__() unknown thread exception");
			}

			try {
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnExit__() begin");
				__OnExit__();
				WAWO_DEBUG("[ThreadObject_Abstract::__RUN()__]__OnExit__() end");
			} catch (wawo::Exception& e) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnExit__ wawo::Exception: %s\n%s[%d] %s\n%s", e.GetInfo() ,e.GetFile(), e.GetLine(), e.GetFunc(), e.GetStackTrace() );
				WAWO_THROW(e);
			} catch( std::exception& e ) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnExit__ exception : %s", e.what() ) ;
				WAWO_THROW(e); //the caller have to check their exceptions
			} catch(...) {
				WAWO_FATAL("[ThreadObject_Abstract::__RUN()__]__OnExit__ unknown thread exception" );
				WAWO_THROW_EXCEPTION("__OnExit__ unknown thread exception");
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
			TR_STOP,
		};
	private:
		ThreadRunObjectState m_state:8;
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
		ThreadRunObjectState GetState() const {
			return m_state;
		}

		inline bool IsRunning() const {
			return m_state == TR_RUNNING ;
		}

		inline bool IsIdle() const {
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

		int Start( bool const& block_start = true ) {
			WAWO_ASSERT( m_state == TR_IDLE );

			if( block_start ) {
				int start_rt = StartThread();
				if( start_rt != wawo::OK ) {
					return start_rt;
				}
				for(int k=0;!IsRunning();k++ ) {
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

		virtual void OnStart() = 0 ;
		virtual void OnStop() = 0 ;
		virtual void Run() = 0;
	public:
		void operator()() {
			WAWO_ASSERT( m_state == TR_START );
			m_state = TR_RUNNING ; //if launch failed, we'll not reach here, and it will not in TR_RUNNING ..., so don't worry
			while( m_state == TR_RUNNING ) {
				Run();
			}
		}
	};
}}
#endif