#ifndef _WAWO_NET_SERVICE_COMMAND_H_
#define _WAWO_NET_SERVICE_COMMAND_H_

namespace wawo { namespace net { namespace service {

	enum EchoCommand {
		C_ECHO_HELLO = 1,
		C_ECHO_STRING_REQUEST_TEST,
		C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES,
		C_ECHO_PINGPONG,
		C_ECHO_SEND_TEST
	};

}}}

#endif