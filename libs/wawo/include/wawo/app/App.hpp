#ifndef _WAWO_APP_APP_HPP_
#define _WAWO_APP_APP_HPP_



#include <wawo/core.h>
#include <wawo/signal/SignalHandler_Abstract.h>
#include <wawo/signal/SignalManager.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/thread/Thread.h>
#include <wawo/time/time.hpp>

#include <set>

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
#ifdef WAWO_PLATFORM_POSIX
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGPIPE, NULL);
#endif
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGINT, this );
			wawo::signal::SignalManager::GetInstance()->RegisterSignal( SIGTERM, this );
		}

		~App() {
		}

		void ProcessSignal( int signo ) {
			WAWO_LOG_INFO( "APP", "receive signo: %d", signo );

#ifdef WAWO_PLATFORM_POSIX
			if( signo == SIGPIPE ) { 
				return ;//IGN
			}
#endif
			m_signalList.insert( signo );
		}

		bool HasReceivedSignal( int signo ) {
			return ( m_signalList.find( signo ) != m_signalList.end() ) ;
		}

		bool ShouldExit() {
			return HasReceivedSignal( SIGINT )||HasReceivedSignal( SIGTERM );
		}

		//in seconds
		int RunUntil( uint32_t time = 0 )
		{

			uint64_t _begin_time = wawo::time::curr_milliseconds() ;

			for( ;; ) {

				wawo::sleep(500);

				if( time != 0 ) {
					uint64_t _t_current_time = wawo::time::curr_milliseconds();
					
					if( (_t_current_time-_begin_time) > time*1000 ) {
						WAWO_LOG_INFO( "APP", "time run: %d", (_t_current_time-_begin_time) );
						return 0;
					}
				}

				if( ShouldExit() ) {
					return 0;
				}
			}

			return -1; //unknown
		}
	};

}}
#endif