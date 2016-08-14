#ifndef _WAWO_SIGNAL_HANDLER_ABSTRACT_H
#define _WAWO_SIGNAL_HANDLER_ABSTRACT_H



namespace wawo { namespace signal {
	
	class SignalHandler_Abstract {

	public:
		virtual ~SignalHandler_Abstract(){};
		virtual void ProcessSignal( int signo ) = 0 ;

	};

} }

#endif