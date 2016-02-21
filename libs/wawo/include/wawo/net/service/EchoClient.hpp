#ifndef _WAWO_NET_CLIENT_SERVICE_ECHO_HPP
#define _WAWO_NET_CLIENT_SERVICE_ECHO_HPP

#include <wawo/net/ServiceProvider_Abstract.hpp>
#include <wawo/net/service/ServiceCommand.h>

namespace wawo { namespace net { namespace service {

	using namespace wawo::net::core;

	template <class _MyPeerT>
	class EchoClient:
		public ServiceProvider_Abstract<_MyPeerT>
	{

		typedef _MyPeerT MyPeerT;
		typedef typename MyPeerT::MyMessageT MyMessageT;
		typedef typename MyPeerT::MySocketT MySocketT;

		typedef typename MyPeerT::MyBasePeerCtxT MyBasePeerCtxT;
		typedef typename MyPeerT::MyBasePeerMessageCtxT MyBasePeerMessageCtxT;

		enum IncreaseMode {
			INC = 0,
			DRE
		};

		byte_t* m_test_to_send_buffer;
		uint32_t m_last_len;
		bool m_increase_mode;
		uint32_t m_max_send_len;
		uint32_t m_min_send_len;

	public:

		EchoClient(uint32_t id):
			ServiceProvider_Abstract<MyPeerT>(id),
			m_last_len(21),
			m_increase_mode(INC),
//			m_max_send_len(1024*1024-20),
			m_max_send_len(39),
//			m_max_send_len(1024*4*4-40), //for send wouldblock test
//			m_max_send_len(1024),
			m_min_send_len(1024)

//			m_min_send_len(1024*3)
		{
			static char s_char_table[] = {
				'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
				'0','1','2','3','4','5','6','7','8','9',
				'\'','!','@','#','$','%','^','&','*','(',')',',','_','+','|','}','{','?','>','<',
				'~','`','[',']',';','/','.',
				'-','='
			};

			uint32_t char_table_len = sizeof( s_char_table)/sizeof(s_char_table[0]) ;
			m_test_to_send_buffer = (byte_t*) malloc ( m_max_send_len * sizeof(byte_t) );

			for(uint32_t i=0;i<m_max_send_len;i++) {
				m_test_to_send_buffer[i] = s_char_table[i%char_table_len] ;
			}
		}

		~EchoClient() {
			WAWO_ASSERT( m_test_to_send_buffer != NULL );
			free( m_test_to_send_buffer );
		}

		virtual void HandleMessage( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) {

			int type = incoming->GetType();
			switch( type ) {
			case wawo::net::message::Wawo::T_SEND:
				{
					OnReceive( ctx, incoming );
				}
				break;
			case wawo::net::message::Wawo::T_REQUEST:
				{
					OnRequest( ctx, incoming );
				}
				break;
			case wawo::net::message::Wawo::T_RESPONSE:
				{
					OnRespond( ctx, incoming );
				}
				break;
			}
		}

		void OnReceive( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ){

		}

		void OnRequest( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) {

		}

		void OnRespond( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) {
			WAWO_ASSERT( incoming->GetId() == this->GetId() );

			WAWO_SHARED_PTR<MyMessageT> const& original = ctx.message ;
			WAWO_REF_PTR<MyPeerT> const& peer = wawo::static_pointer_cast<MyPeerT>(ctx.peer);

			WAWO_SHARED_PTR<Packet> const& packet = incoming->GetPacket();
			uint32_t command = packet->Read<uint32_t>();

			switch( command ) {
			case wawo::net::service::C_ECHO_HELLO:
			case wawo::net::service::C_ECHO_STRING_REQUEST_TEST:
			case wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES:
				{

					uint32_t code = packet->Read<uint32_t>();
					WAWO_CONDITION_CHECK( code == wawo::OK );

					uint32_t receive_hello_len = packet->Read<uint32_t>();
					byte_t* received_hello_str = (byte_t*) malloc( receive_hello_len*sizeof(byte_t) );
					WAWO_CONDITION_CHECK( packet->Length() == receive_hello_len );
					packet->Read( received_hello_str, receive_hello_len );

					WAWO_SHARED_PTR<Packet> const& original_packet = original->GetPacket();
					uint32_t original_cmd = original_packet->Read<uint32_t>();
					WAWO_ASSERT( original_cmd == command );

					uint32_t original_hello_len = original_packet->Read<uint32_t>();
					WAWO_CONDITION_CHECK( receive_hello_len == original_hello_len );
					WAWO_CONDITION_CHECK( original_packet->Length() == original_hello_len );

					WAWO_CONDITION_CHECK( (strncmp( (char const*) received_hello_str, (char const*) original_packet->Begin() , original_hello_len ) == 0) );
					free( received_hello_str );


					if( command == wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES ) {
						return ;
					}

					//WAWO_LOG_INFO("echo_client", "received: %s", (char const* const)(hello_cstring) );
					//resend
					//int cid = wawo::net::client::service::Echo<ClientType>::C_TEST ;
					WAWO_SHARED_PTR<Packet> packet_o( new Packet(256) );
					packet_o->Write<uint32_t>(wawo::net::service::C_ECHO_STRING_REQUEST_TEST);
					uint32_t rand_len = (++m_last_len%m_max_send_len);

					packet_o->Write<uint32_t>( rand_len );
					packet_o->Write( (byte_t*) m_test_to_send_buffer, rand_len );

					WAWO_SHARED_PTR<Packet> packet_ss( new Packet( *packet_o ) );
					WAWO_SHARED_PTR<MyMessageT> message_ss( new MyMessageT( wawo::net::WSI_ECHO, packet_ss ) );

					int rt;
		//#define TEST_SEND
		#ifdef TEST_SEND
					rt = client->Send( message_ss );
					WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
		#endif

					WAWO_SHARED_PTR<Packet> packet_t1( new Packet( *packet_o ) );
					WAWO_SHARED_PTR<MyMessageT> message_r1( new MyMessageT( wawo::net::WSI_ECHO, packet_t1 ) );
					rt = peer->Request( message_r1 );
					WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

#ifdef TEST_PRESSES
					WAWO_REF_PTR<Packet> packet_press( new Packet( 256 ) );
					packet_press->Write<uint32_t>(wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES);
					packet_press->Write<uint32_t>( rand_len );
					packet_press->Write( (byte_t*) m_test_to_send_buffer, rand_len );

					WAWO_REF_PTR<MyMessageT> message_press( new MyMessageT( wawo::net::WSI_ECHO, packet_press ) );
					rt = client->Request( message_press );
					WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif

		#ifdef TEST_SEND
					//rt = client->Send( message_ss );
					//WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
#endif
				}
				break;
			case wawo::net::service::C_ECHO_PINGPONG:
				{
					uint32_t code = packet->Read<uint32_t>();
					WAWO_ASSERT( code == wawo::OK );

					peer->Echo_RequestHello();
				}
				break;
			default:
				{
					WAWO_ASSERT( "invalid respond command id" );
				}
				break;
			}
		}
	};

}}}

#endif
