#ifndef _WAWO_NET_SERVER_SERVICE_ECHO_SERVER_HPP
#define _WAWO_NET_SERVER_SERVICE_ECHO_SERVER_HPP

#include <atomic>

#include <wawo/net/ServiceProvider_Abstract.hpp>
#include <wawo/net/service/ServiceCommand.h>

namespace wawo { namespace net { namespace service {

	using namespace wawo::net::core;

	template <class _MyPeerT>
	class EchoServer:
		public wawo::net::ServiceProvider_Abstract<_MyPeerT>
	{

		typedef _MyPeerT MyPeerT;
		typedef typename MyPeerT::MyMessageT MyMessageT;
		typedef typename MyPeerT::MySocketT MySocketT;

		typedef typename MyPeerT::MyBasePeerCtxT MyBasePeerCtxT;
		typedef typename MyPeerT::MyBasePeerMessageCtxT MyBasePeerMessageCtxT;
	public:
		EchoServer(int id ):
			ServiceProvider_Abstract<_MyPeerT>(id),
			m_total(0),
			m_request_count(0),
			m_check_millseconds( wawo::time::curr_milliseconds() ),
			m_check_millseconds_begin(wawo::time::curr_milliseconds())
		{
		}
		~EchoServer() {}

		virtual void HandleMessage( MyBasePeerMessageCtxT const& ctx , WAWO_SHARED_PTR<MyMessageT> const& incoming ) {

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

		virtual void OnReceive( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ){

		}

		virtual void OnRequest(  MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) {

			WAWO_REF_PTR<MyPeerT> peer = WAWO_REF_PTR<MyPeerT>( static_cast<MyPeerT*> (ctx.peer.Get()) );

			WAWO_SHARED_PTR<Packet> const& packet = incoming->GetPacket();
			uint32_t command = packet->Read<uint32_t>();

			if( command == wawo::net::service::C_ECHO_HELLO ||
				command == wawo::net::service::C_ECHO_STRING_REQUEST_TEST ||
				command == wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES
			) {
				uint32_t hello_len = packet->Read<uint32_t>();
				byte_t* hello_string = (byte_t*) malloc( hello_len*sizeof(byte_t) );
				packet->Read( hello_string, hello_len );

				//WAWO_LOG_DEBUG("echo_server", "received: %s", (char const* const)(hello_string) );

				WAWO_SHARED_PTR<Packet> packet_o( new Packet(256) );
				packet_o->Write<uint32_t>( command );
				packet_o->Write<uint32_t>(wawo::OK);

				packet_o->Write<uint32_t>(hello_len);
				packet_o->Write(hello_string, hello_len);
				free(hello_string);

				WAWO_SHARED_PTR<MyMessageT> message( new MyMessageT( this->GetId(), packet_o ) );
				int rt = peer->Respond( message, incoming, ctx );


				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				if( rt == wawo::OK ) {
					m_request_count ++ ;
				}

				if ( (m_request_count%50000) == 0 ) {
					uint64_t current_millsecond = wawo::time::curr_milliseconds();
					uint64_t diff_time = current_millsecond - m_check_millseconds ;
					if( diff_time != 0 ) {
						double qps = (m_request_count/diff_time ) * 1000.0;
						m_total += m_request_count;

						uint64_t diff_time_total = current_millsecond - m_check_millseconds_begin;
						double qps_avg = (m_total/diff_time_total) * 1000.0;

						m_request_count = 0 ;

						m_check_millseconds = current_millsecond ;
						WAWO_LOG_WARN( "echo", "current qps:%0.f\tavg qps:%0.f\ttotal:%d\ttime cost:%0.2f\thours%", qps, qps_avg,m_total, (diff_time_total/(3600.0*1000)) );
					}
				}
			} else if( command == wawo::net::service::C_ECHO_PINGPONG ) {

				WAWO_SHARED_PTR<Packet> packet_o( new Packet(256) );
				packet_o->Write<uint32_t>(command);
				packet_o->Write<uint32_t>(wawo::OK);
				WAWO_SHARED_PTR<MyMessageT> message( new MyMessageT( this->GetId(), packet_o ) );
				int rt = peer->Respond( message, incoming, ctx );
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			} else {
				WAWO_THROW_EXCEPTION("invalid echo command");
			}
		}

		virtual void OnRespond( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming  ) {
		}

	private:
		uint32_t m_total;
		std::atomic<uint32_t> m_request_count;
		uint64_t m_check_millseconds;
		uint64_t m_check_millseconds_begin;
	};

}}}

#endif
