#ifndef _WAWO_SIGNAL_HANDLER_ABSTRACT_H
#define _WAWO_SIGNAL_HANDLER_ABSTRACT_H



namespace wawo { namespace signal {
	
	class signal_handler_abstract {

	public:
		virtual ~signal_handler_abstract(){};
		virtual void handle_signal( int signo ) = 0 ;

	};

} }

#endif