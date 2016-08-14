#ifndef _SERVICES_ECHO_CLIENT_H_
#define _SERVICES_ECHO_CLIENT_H_

#include <wawo.h>
#include "ServiceCommand.h"

namespace services {

	using namespace wawo;
	using namespace wawo::algorithm;

	template <class _MyPeerT>
	class EchoClient:
		public wawo::net::ServiceProvider_Abstract<_MyPeerT>
	{
		//for gcc...
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::MessageT MyMessageT;

		enum IncreaseMode {
			INC = 0,
			DRE
		};

		bool m_increase_mode;
		byte_t* m_test_to_send_buffer;
		u32_t m_last_len;
		u32_t m_min_send_len;
		u32_t m_max_send_len;

	public:

		EchoClient(u32_t id):
			ServiceProvider_Abstract<MyPeerT>(id),
			m_last_len(21),
			m_increase_mode(INC),
			m_min_send_len(0),
//			m_min_send_len(1024*3),
//			m_max_send_len(1024*1024-20),
			m_max_send_len(64)
//			m_max_send_len(1024*4*4-40), //for send wouldblock test
//			m_max_send_len(1024)
		{
			static char s_char_table[] = {
				'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
				'0','1','2','3','4','5','6','7','8','9',
				'\'','!','@','#','$','%','^','&','*','(',')',',','_','+','|','}','{','?','>','<',
				'~','`','[',']',';','/','.',
				'-','='
			};

			u32_t char_table_len = sizeof( s_char_table)/sizeof(s_char_table[0]) ;
			m_test_to_send_buffer = (byte_t*) malloc ( m_max_send_len * sizeof(byte_t) );

			for(u32_t i=0;i<m_max_send_len;i++) {
				m_test_to_send_buffer[i] = s_char_table[i%char_table_len] ;
			}
		}

		~EchoClient() {
			WAWO_ASSERT( m_test_to_send_buffer != NULL );
			free( m_test_to_send_buffer );
		}

		virtual void WaitMessage( WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {

			int type = incoming->GetType();
			switch( type ) {
			case wawo::net::peer::message::Wawo::T_SEND:
				{
					OnReceive(peer, incoming );
				}
				break;
			case wawo::net::peer::message::Wawo::T_REQUEST:
				{
					OnRequest(peer, incoming );
				}
				break;
			case wawo::net::peer::message::Wawo::T_RESPONSE:
				{
					OnRespond(peer, incoming );
				}
				break;
			}
		}

		void OnReceive(WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {
		}

		void OnRequest(WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {
		}

		void OnRespond(WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {
			HandlePeerResp(peer,incoming);
		}

		void HandlePeerResp(WWRP<PeerT> const& peer, WWSP<MyMessageT> const& message) {
			WWSP<Packet> const& packet = message->GetPacket();
			u32_t command = packet->Read<u32_t>();

			switch (command) {
			case services::C_ECHO_HELLO:
			case services::C_ECHO_STRING_REQUEST_TEST:
			case services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES:
			{

				u32_t code = packet->Read<u32_t>();
				WAWO_CONDITION_CHECK(code == wawo::OK);

				u32_t receive_hello_len = packet->Read<u32_t>();
				byte_t* received_hello_str = NULL;

				if (receive_hello_len > 0) {
					received_hello_str = (byte_t*)malloc(receive_hello_len * sizeof(byte_t));
					WAWO_CONDITION_CHECK(packet->Length() == receive_hello_len);
					packet->Read(received_hello_str, receive_hello_len);
					free(received_hello_str);
				}

				if (command == services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES) {
					return;
				}

				//resend
				//int cid = wawo::net::client::service::Echo<ClientType>::C_TEST ;
				WWSP<Packet> packet_o(new Packet(256));
				packet_o->Write<u32_t>(services::C_ECHO_STRING_REQUEST_TEST);
				u32_t rand_len = (++m_last_len%m_max_send_len);

				packet_o->Write<u32_t>(rand_len);
				packet_o->Write((byte_t*)m_test_to_send_buffer, rand_len);

				WWSP<Packet> packet_t1(new Packet(*packet_o));
				packet_t1->WriteLeft < wawo::net::ServiceIdT >(services::S_ECHO);
				WWSP<MyMessageT> message_r1(new MyMessageT(packet_t1));
				int rt;
				rt = peer->Request(message_r1);
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

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
				u32_t code = packet->Read<u32_t>();
				WAWO_ASSERT(code == wawo::OK);

				int rt;
				PEER_SND_HELLO(peer, rt);
			}
			break;
			default:
			{
				WAWO_ASSERT("invalid respond command id");
			}
			break;
			}
		}

	};

}

#endif