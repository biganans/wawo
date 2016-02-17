#ifndef _WAWO_NET_SERVICE_LIST_H_
#define _WAWO_NET_SERVICE_LIST_H_



#define WAWO_ENABLE_ECHO_TEST 1


namespace wawo { namespace net {

	enum ServiceId {
		WSI_WAWO = 0 , //default reserved service
		WSI_AUTH,
		WSI_ECHO,
		WSI_ECHO_JUST_PRESS,
		WSI_LOBBY,
		WSI_MAX
	};
}}


#if WAWO_ENABLE_ECHO_TEST
	#include <wawo/net/service/EchoClient.hpp>
	#include <wawo/net/service/EchoServer.hpp>
#endif

#endif
