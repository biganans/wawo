#include <wawo/core.hpp>

#include <signal.h>
#include <wawo/signal/signal_manager.h>
#include <wawo/log/logger_manager.h>
#include <wawo/signal/signal_handler_abstract.h>

namespace wawo { namespace signal {

	

	signal_manager::signal_manager():
		m_signalHandlersMutex(),
		m_signalHandlers()
	{

#if WAWO_PLATFORM_WIN
		int rt = _RegisterCtrlEventHandler();

		WAWO_ASSERT( rt == 0 );
		(void)rt;
#endif
	}

	signal_manager::~signal_manager() {

		if( m_signalHandlers.size() ) {

			SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it ;
			for( it = m_signalHandlers.begin() ; it != m_signalHandlers.end(); ) {

				it->second->clear();
				delete it->second ;

				m_signalHandlers.erase(it++);
			}
		}

#if WAWO_PLATFORM_WIN
		_UnRegisterCtrlEventHandler();
#endif

	}

	void signal_manager::raise_signal( int signo ) {
		_signal_dispatch( signo );
	}


	int signal_manager::register_signal(int signo , signal_handler_abstract* signalHandler) {

#if WAWO_PLATFORM_WIN
		if( signo == SIGINT ) {
			//WAWO_WARN( "[signal_manager]WIN32 do not support SIGINT, USE CTRL_C_EVENT, or CTRL_BREAK_EVENT" );
		}
#endif


		if( signalHandler == NULL ) {
			::signal( signo,SIG_IGN );
			return wawo::OK;
		}

		lock_guard<mutex> _lg(m_signalHandlersMutex);
		::signal( signo, signal_manager::signal_dispatch ) ;

		SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it = m_signalHandlers.find(signo);

		if( it == m_signalHandlers.end() ) {
			//none entry right now

			SIGNAL_HANDLER_VECTOR* handlersVector =  new SIGNAL_HANDLER_VECTOR();
			handlersVector->push_back( signalHandler );

			m_signalHandlers.insert( std::make_pair(signo, handlersVector) );
			//WAWO_WARN( "[signal_manager]register signo handler: %d", signo );

		} else {
			//WAWO_WARN( "[signal_manager]register signo handler: %d, add", signo );
			it->second->push_back( signalHandler );
		}

		return wawo::OK ;
	}

#if WAWO_PLATFORM_WIN
	int signal_manager::_RegisterCtrlEventHandler() {

		if( SetConsoleCtrlHandler( (PHANDLER_ROUTINE) signal_manager::CtrlEvent_Handler , TRUE ) == FALSE ) {
			WAWO_CORE( "[signal_manager]SetConsoleCtrlHandler failed, errno: %d", GetLastError() );
			return -1;
		}

		return 0 ;
	}
	int signal_manager::_UnRegisterCtrlEventHandler() {

		if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)signal_manager::CtrlEvent_Handler, false) == FALSE) {
			WAWO_CORE("[signal_manager]SetConsoleCtrlHandler failed, errno: %d", GetLastError());
			return -1;
		}

		return 0;
	}
	BOOL WINAPI signal_manager::CtrlEvent_Handler( DWORD CtrlType ) {

		//windows ctrl event

		switch( CtrlType ) {

			case CTRL_C_EVENT:
				{
					WAWO_WARN( "[signal_manager]CTRL_C_EVENT received" );
					raise(SIGINT);
				}
				break;
			case CTRL_BREAK_EVENT:
				{
					WAWO_WARN( "[signal_manager]CTRL_BREAK_EVENT received" );
					raise(SIGINT);
				}
				break;

			case CTRL_SHUTDOWN_EVENT:
				{
					WAWO_WARN( "[signal_manager]CTRL_SHUTDOWN_EVENT received" );
					raise(SIGTERM);
				}
				break;
			case CTRL_CLOSE_EVENT:
				{
					WAWO_WARN( "[signal_manager]CTRL_CLOSE_EVENT received" );
					raise(SIGTERM);
				}
				break;

			case CTRL_LOGOFF_EVENT:
				{
   					WAWO_WARN( "[signal_manager]CTRL_LOGOFF_EVENT received" );
					raise(SIGTERM);
				}
				break;

			default:
				{
					WAWO_WARN("[signal_manager]unknown windows event received");
					return false;
				}
		}

		return true;
	}

#endif

	void signal_manager::signal_dispatch( int signo ) {
		signal_manager::instance()->_signal_dispatch( signo );
	}

	void signal_manager::_signal_dispatch( int signo ) {

		lock_guard<mutex> _lg( m_signalHandlersMutex );

		SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it = m_signalHandlers.find( signo );

		if( it == m_signalHandlers.end() ) {

			WAWO_WARN( "[signal_manager]no hander for signo: %d", signo );
			return ;
		}

		SIGNAL_HANDLER_VECTOR *signalHandlers = it->second ;
		SIGNAL_HANDLER_VECTOR::iterator it_handlers = signalHandlers->begin();

		while( it_handlers != signalHandlers->end() ) {
			(*it_handlers)->handle_signal( signo );
			it_handlers++;
		}
	}
}}