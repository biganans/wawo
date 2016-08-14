#ifndef _SERVICES_COMMAND_H_
#define _SERVICES_COMMAND_H_

#include <wawo.h>
namespace services {
	enum ServicesId {
		S_ECHO = wawo::net::S_CUSTOM_ID_BEGIN
	};

	enum EchoCommand {
		C_ECHO_HELLO = 1,
		C_ECHO_STRING_REQUEST_TEST,
		C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES,
		C_ECHO_PINGPONG,
		C_ECHO_SEND_TEST
	};
} 


#define PEER_SND_HELLO(peer,rt) \
	do { \
		WWSP<wawo::algorithm::Packet> packet(new wawo::algorithm::Packet(256)); \
		wawo::Len_CStr hello_string = "hello server"; \
		packet->Write<wawo::u32_t>(services::C_ECHO_HELLO); \
		packet->Write<wawo::u32_t>(hello_string.Len()); \
		packet->Write((wawo::byte_t const* const)hello_string.CStr(), hello_string.Len()); \
		packet->WriteLeft<wawo::net::ServiceIdT>(services::S_ECHO); \
		WWSP<typename PeerT::MessageT> message(new typename PeerT::MessageT(packet)); \
		rt=peer->Request(message); \
	} while (0) ;

#endif