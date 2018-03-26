#ifndef _SERVICES_COMMAND_H_
#define _SERVICES_COMMAND_H_

#include <wawo.h>
namespace services {

	enum ServicesId {
		S_ECHO = 100
	};

	enum EchoCommand {
		C_ECHO_HELLO = 1,
		C_ECHO_STRING_REQUEST_TEST,
		C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES,
		C_ECHO_PINGPONG,
		C_ECHO_SEND_TEST
	};

	enum IncreaseMode {
		INC = 0,
		DRE
	};

	const wawo::u32_t MAX_SND_LEN		= 64;
	wawo::u32_t LAST_LEN				= 64;
	static wawo::byte_t* bytes_buffer	= NULL ;

	void InitSndBytes() {
		static char s_char_table[] = {
			'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
			'0','1','2','3','4','5','6','7','8','9',
			'\'','!','@','#','$','%','^','&','*','(',')',',','_','+','|','}','{','?','>','<',
			'~','`','[',']',';','/','.',
			'-','='
		};
		wawo::u32_t char_table_len = sizeof(s_char_table) / sizeof(s_char_table[0]);
		bytes_buffer = (wawo::byte_t*)malloc(MAX_SND_LEN * sizeof(wawo::byte_t));

		for (wawo::u32_t i = 0; i<MAX_SND_LEN; i++) {
			bytes_buffer[i] = s_char_table[i%char_table_len];
		}
	}

	void DeinitSndBytes() {
		free(bytes_buffer);
	}

	class HelloProcessor {

	public:

		HelloProcessor()
		{
		}

		void HandleResp(WWRP<wawo::net::socket_handler_context> const& ctx, WWSP<wawo::packet> const& resp_pack ) {
			WWSP<wawo::packet> const& packet = resp_pack;
			wawo::u32_t command = packet->read<wawo::u32_t>();

			switch (command) {
			case services::C_ECHO_HELLO:
			case services::C_ECHO_STRING_REQUEST_TEST:
			case services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES:
			{
				wawo::u32_t code = packet->read<wawo::u32_t>();
				WAWO_CONDITION_CHECK(code == wawo::OK);

				wawo::u32_t receive_hello_len = packet->read<wawo::u32_t>();
				wawo::byte_t* received_hello_str = NULL;

				if (receive_hello_len > 0) {
					received_hello_str = (wawo::byte_t*)malloc(receive_hello_len * sizeof(wawo::byte_t));
					WAWO_CONDITION_CHECK(packet->len() == receive_hello_len);
					packet->read(received_hello_str, receive_hello_len);
					free(received_hello_str);
				}

				if (command == services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES) {
					return;
				}

				//resend
				//int cid = wawo::net::client::service::Echo<ClientType>::C_TEST ;
				WWSP<wawo::packet> packet_o = wawo::make_shared<wawo::packet>(256);
				packet_o->write<wawo::u32_t>(services::C_ECHO_STRING_REQUEST_TEST);
				wawo::u32_t to_snd_len = (++LAST_LEN%MAX_SND_LEN);

				packet_o->write<wawo::u32_t>(to_snd_len);
				packet_o->write((wawo::byte_t*)bytes_buffer, to_snd_len);

				WWSP<wawo::packet> packet_t1 = wawo::make_shared<wawo::packet>(*packet_o);

				packet_t1->write_left < wawo::u8_t >(services::S_ECHO);
				ctx->write(packet_t1);

//				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

#ifdef TEST_SEND
				WWSP<Packet> packet_ss(new Packet(*packet_o));
				packet_ss->WriteLeft<wawo::net::ServiceIdT>(services::S_ECHO);

				WWSP<MyMessageT> message_ss(new MyMessageT(packet_ss));

				rt = client->Send(message_ss);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif

#ifdef TEST_PRESSES
				WWRP<Packet> packet_press(new Packet(256));
				packet_press->Write<u32_t>(wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES);
				packet_press->Write<u32_t>(rand_len);
				packet_press->Write((byte_t*)m_test_to_send_buffer, rand_len);

				WWRP<MyMessageT> message_press(new MyMessageT(wawo::net::WSI_ECHO, packet_press));
				rt = client->Request(message_press);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif

#ifdef TEST_SEND
				//rt = client->Send( message_ss );
				//WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif
			}
			break;
			case services::C_ECHO_PINGPONG:
			{
				wawo::u32_t code = packet->read<wawo::u32_t>();
				WAWO_ASSERT(code == wawo::OK);

				//int rt;
				//PEER_REQ_HELLO(peer, rt);

				SendHello(ctx);
			}
			break;
			default:
			{
				WAWO_ASSERT( !"invalid respond command id");
			}
			break;
			}
		}

		static void SendHello(WWRP<wawo::net::socket_handler_context> const& ctx) {

			WWSP<wawo::packet> packet(new wawo::packet(256));
			wawo::len_cstr hello_string = "hello server";
			packet->write<wawo::u32_t>(services::C_ECHO_HELLO);
			packet->write<wawo::u32_t>(hello_string.len);
			packet->write((wawo::byte_t const* const)hello_string.cstr, hello_string.len);

			packet->write_left<wawo::u8_t>(services::S_ECHO);
			ctx->write(packet);
		}
	};
}

#endif