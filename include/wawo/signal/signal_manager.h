#ifndef _WAWO_SIGNAL_MANAGER_H
#define _WAWO_SIGNAL_MANAGER_H

#include <map>
#include <vector>

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>
#include <wawo/signal/signal_handler_abstract.h>

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

	typedef std::vector<signal_handler_abstract*> SIGNAL_HANDLER_VECTOR;
	typedef std::map<int,SIGNAL_HANDLER_VECTOR*> SIGNAL_AND_SIGNAL_HANDLER_MAP ;

}}

#define WAWO_SIGNAL_MANAGER_MAX_SIGNAL_HANDLER	10



#include <signal.h>
#include <vector>

#include <wawo/singleton.hpp>
#include <wawo/thread/mutex.hpp>

namespace wawo { namespace signal {

	class signal_manager: public wawo::singleton<signal_manager> {

	protected:
		signal_manager();
		~signal_manager();

	public:
		void raise_signal( int signo );//simulate signo trigger
		int register_signal( int signo, signal_handler_abstract* signalHandler ) ;
		static void signal_dispatch( int signo );

	protected:
		void _signal_dispatch( int signo );

#ifdef WAWO_PLATFORM_WIN
		//WINDOWS ONLY
		static BOOL WINAPI CtrlEvent_Handler( DWORD CtrlType );
#endif
	private:
#ifdef WAWO_PLATFORM_WIN
		//WINDOWS ONLY
		int _RegisterCtrlEventHandler();
		int _UnRegisterCtrlEventHandler();
#endif
	private:
		void handle_signal( int signo );

		wawo::thread::mutex m_signalHandlersMutex;
		SIGNAL_AND_SIGNAL_HANDLER_MAP m_signalHandlers;

		DECLARE_SINGLETON_FRIEND(signal_manager);
	};
}}

#define SIGNAL_MANAGER_INSTANCE (wawo::signal_manager::instance())
#endif