#ifndef _WAWO_SIGNAL_MANAGER_H
#define _WAWO_SIGNAL_MANAGER_H


#include <wawo/core.h>
#include <wawo/SafeSingleton.hpp>
#include <wawo/signal/SignalHandler_Abstract.h>

#include <map>
#include <vector>

namespace wawo { namespace signal {

	
	/*
	struct Signals {	


		enum Signal {
			WAWO_SIGABRT,
			WAWO_SIGFPE,
			WAWO_SIGILL,
			WAWO_SIGINT, //equal to CTRL_C_BREAK, CTRL_EVENT in windows
			WAWO_CTRL_C_BREAK, //windows only
			WAWO_CTRL_BREAK_EVENT, //windows only
			WAWO_SIGTERM,
			WAWO_SIGSEGV,
			WAWO_SIGUNNOWN
		};
	};
	*/

	typedef std::vector<SignalHandler_Abstract*> SIGNAL_HANDLER_VECTOR;
	typedef std::map<int,SIGNAL_HANDLER_VECTOR*> SIGNAL_AND_SIGNAL_HANDLER_MAP ;

}}

#define WAWO_SIGNAL_MANAGER_MAX_SIGNAL_HANDLER	10



#include <signal.h>
#include <vector>

#include <wawo/SafeSingleton.hpp>
#include <wawo/thread/Mutex.h>

namespace wawo { namespace signal {

	class SignalManager: public wawo::SafeSingleton<SignalManager> {

	protected:
		SignalManager();
		~SignalManager();

	public:
		void RaiseSignal( int signo );//simulate signo trigger
		int RegisterSignal( int signo, SignalHandler_Abstract* signalHandler ) ;
		static void SignalDispatch( int signo );

	protected:
		void _SignalDispatch( int signo );

#ifdef WAWO_PLATFORM_WIN
		//WINDOWS ONLY
		static BOOL WINAPI CtrlEvent_Handler( DWORD CtrlType );
#endif
	private:
#ifdef WAWO_PLATFORM_WIN
		//WINDOWS ONLY
		int _RegisterCtrlEventHandler();
#endif
	private:
		void ProcessSignal( int signo );
		
		wawo::thread::Mutex m_signalHandlersMutex;
		SIGNAL_AND_SIGNAL_HANDLER_MAP m_signalHandlers;



		DECLARE_SAFE_SINGLETON_FRIEND(SignalManager);
	};

} }

#define SIGNAL_MANAGER_INSTANCE (wawo::SignalManager::GetInstance())
#endif