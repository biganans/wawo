#ifndef _WAWO_APP_APP_HPP_
#define _WAWO_APP_APP_HPP_

#include <set>

#include <wawo/core.h>
#include <wawo/signal/SignalHandler_Abstract.h>
#include <wawo/signal/SignalManager.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/time/time.hpp>

namespace wawo { namespace app {

	class App :
		public wawo::signal::SignalHandler_Abstract
	{

	private:
	std::set<int> m_signalList;

	public:
		App() :
			SignalHandler_Abstract()
		{
			WWRP<wawo::log::Logger_Abstract> fileLogger (new wawo::log::FileLogger("./wawo.log")) ;
			fileLogger->SetLevel ( WAWO_FILE_LOGGER_LEVEL );
			wawo::log::LoggerManager::GetInstance()->AddLogger( fileLogger );

#ifdef WAWO_PLATFORM_GNU
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGPIPE, NULL);
#endif
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGINT, this );
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGTERM, this );

			int rt = WAWO_NET_CORE_IO_TASK_MANAGER->Start();
			WAWO_CONDITION_CHECK( rt == wawo::OK );
		}

		~App() 
		{
			WAWO_NET_CORE_IO_TASK_MANAGER->Stop();
		}

		void CancelAllTask() {
			WAWO_NET_CORE_IO_TASK_MANAGER->Stop();
		}

		void RaiseSignal( int signo ) {
			wawo::signal::SignalManager::GetInstance()->RaiseSignal(signo);
		}

		void ProcessSignal( int signo ) {
			WAWO_INFO( "[APP]receive signo: %d", signo );

#ifdef WAWO_PLATFORM_GNU
			if( signo == SIGPIPE ) {
				return ;//IGN
			}
#endif
			m_signalList.insert( signo );
		}

		bool HasReceivedSignal( int signo ) {
			return ( m_signalList.find( signo ) != m_signalList.end() ) ;
		}

		bool HasReceivedSignalExit() {
			return HasReceivedSignal( SIGINT )||HasReceivedSignal( SIGTERM );
		}

		//in seconds
		int RunUntil( u32_t time = 0 )
		{
			u64_t _begin_time = wawo::time::curr_milliseconds() ;
			for( ;; ) {
				wawo::sleep(1000);
				if( time != 0 ) {
					u64_t _t_current_time = wawo::time::curr_milliseconds();
					if( (_t_current_time-_begin_time) > time*1000 ) {
						WAWO_INFO( "[APP]time run: %d", (_t_current_time-_begin_time) );
						return 0;
					}
				}
				if( HasReceivedSignalExit() ) {
					return 0;
				}
			}
			return -1; //unknown
		}
	};

}}
#endif
