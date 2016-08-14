#include <wawo/core.h>

#include <signal.h>
#include <wawo/signal/SignalManager.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/signal/SignalHandler_Abstract.h>

namespace wawo { namespace signal {

	using namespace wawo::thread;

	SignalManager::SignalManager():
		m_signalHandlersMutex(),
		m_signalHandlers()
	{

#if WAWO_PLATFORM_WIN
		int rt = _RegisterCtrlEventHandler();

		WAWO_ASSERT( rt == 0 );
#endif
	}

	SignalManager::~SignalManager() {


		if( m_signalHandlers.size() ) {

			SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it ;
			for( it = m_signalHandlers.begin() ; it != m_signalHandlers.end(); ) {

				it->second->clear();
				delete it->second ;

				m_signalHandlers.erase(it++);
			}
		}

	}

	void SignalManager::RaiseSignal( int signo ) {
		_SignalDispatch( signo );
	}


	int SignalManager::RegisterSignal(int signo , SignalHandler_Abstract* signalHandler) {

#if WAWO_PLATFORM_WIN
		if( signo == SIGINT ) {
			WAWO_WARN( "[SignalManager]WIN32 do not support SIGINT, USE CTRL_C_EVENT, or CTRL_BREAK_EVENT" );
		}
#endif


		if( signalHandler == NULL ) {
			::signal( signo,SIG_IGN );
			return wawo::OK;
		}

		LockGuard<Mutex> _lg(m_signalHandlersMutex);
		::signal( signo, SignalManager::SignalDispatch ) ;

		SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it = m_signalHandlers.find(signo);

		if( it == m_signalHandlers.end() ) {
			//none entry right now

			SIGNAL_HANDLER_VECTOR* handlersVector =  new SIGNAL_HANDLER_VECTOR();
			handlersVector->push_back( signalHandler );

			m_signalHandlers.insert( std::make_pair(signo, handlersVector) );
			WAWO_WARN( "[SignalManager]register signo handler: %d", signo );

		} else {
			WAWO_WARN( "[SignalManager]register signo handler: %d, add", signo );
			it->second->push_back( signalHandler );
		}

		return wawo::OK ;

	}

#if WAWO_PLATFORM_WIN
	int SignalManager::_RegisterCtrlEventHandler() {

		if( SetConsoleCtrlHandler( (PHANDLER_ROUTINE) SignalManager::CtrlEvent_Handler , TRUE ) == FALSE ) {
			WAWO_CORE( "[SignalManager]SetConsoleCtrlHandler failed, errno: %d", GetLastError() );
			return -1;
		}

		return 0 ;
	}

	BOOL WINAPI SignalManager::CtrlEvent_Handler( DWORD CtrlType ) {

		//windows ctrl event

		switch( CtrlType ) {

			case CTRL_C_EVENT:
				{
					raise( SIGINT );
					WAWO_WARN( "[SignalManager]CTRL_C_EVENT received" );
				}
				break;
			case CTRL_BREAK_EVENT:
				{
					raise( SIGINT );
					WAWO_WARN( "[SignalManager]CTRL_BREAK_EVENT received" );
				}
				break;

			case CTRL_SHUTDOWN_EVENT:
				{
					raise( SIGTERM );
					WAWO_WARN( "[SignalManager]CTRL_SHUTDOWN_EVENT received" );
				}
				break;
			case CTRL_CLOSE_EVENT:
				{
					raise( SIGTERM );
					WAWO_WARN( "[SignalManager]CTRL_CLOSE_EVENT received" );
				}
				break;

			case CTRL_LOGOFF_EVENT:
				{
					raise( SIGTERM );
   					WAWO_WARN( "[SignalManager]CTRL_LOGOFF_EVENT received" );
				}
				break;

			default:
				{
					return false;
				}
		}

		return true;
	}

#endif

	void SignalManager::SignalDispatch( int signo ) {
		SignalManager::GetInstance()->_SignalDispatch( signo );
	}

	void SignalManager::_SignalDispatch( int signo ) {

		LockGuard<Mutex> _lg( m_signalHandlersMutex );

		SIGNAL_AND_SIGNAL_HANDLER_MAP::iterator it = m_signalHandlers.find( signo );

		if( it == m_signalHandlers.end() ) {

			WAWO_WARN( "[SignalManager]no hander for signo: %d", signo );
			return ;
		}

		SIGNAL_HANDLER_VECTOR *signalHandlers = it->second ;
		SIGNAL_HANDLER_VECTOR::iterator it_handlers = signalHandlers->begin();

		while( it_handlers != signalHandlers->end() ) {
			(*it_handlers)->ProcessSignal( signo );
			it_handlers++;
		}

	}

} }
