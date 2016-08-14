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
		typedef typename ServiceProvider_Abstract<_MyPeerT>::PeerMessageCtx PeerMessageCtx;
		typedef typename _MyPeerT::MessageT MyMessageT;

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

		virtual void WaitMessage( PeerMessageCtx const& ctx , WWSP<MyMessageT> const& incoming ) {

			int type = incoming->GetType();
			switch( type ) {
			case wawo::net::peer::message::Wawo::T_SEND:
				{
					OnReceive( ctx, incoming );
				}
				break;
			case wawo::net::peer::message::Wawo::T_REQUEST:
				{
					OnRequest( ctx, incoming );
				}
				break;
			case wawo::net::peer::message::Wawo::T_RESPONSE:
				{
					OnRespond( ctx, incoming );
				}
				break;
			}
		}

		virtual void OnReceive( PeerMessageCtx const& ctx, WWSP<MyMessageT> const& incoming ){
			(void)ctx;
			(void)incoming;
		}

		virtual void OnRequest(  PeerMessageCtx const& ctx, WWSP<MyMessageT> const& incoming ) {

			WWSP<Packet> const& packet = incoming->GetPacket();

			uint32_t command = packet->Read<uint32_t>();

			if( command == wawo::net::service::C_ECHO_HELLO ||
				command == wawo::net::service::C_ECHO_STRING_REQUEST_TEST ||
				command == wawo::net::service::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES
			) {
				uint32_t hello_len = packet->Read<uint32_t>();
				byte_t* hello_string = NULL;

				if (hello_len > 0) {
					hello_string = (byte_t*)malloc(hello_len * sizeof(byte_t));
					packet->Read(hello_string, hello_len);
				}

				WWSP<Packet> packet_o( new Packet(256) );
				packet_o->Write<uint32_t>( command );
				packet_o->Write<uint32_t>(wawo::OK);
				packet_o->Write<uint32_t>(hello_len);

				if (hello_len > 0) {
					packet_o->Write(hello_string, hello_len);
					free(hello_string);
				}

				packet_o->WriteLeft<ServiceIdT>(this->GetId());
				WWSP<MyMessageT> message( new MyMessageT( packet_o ) );
				int rt = ctx.peer->Respond( message, incoming );


				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				if( rt == wawo::OK ) {
					m_request_count ++ ;
				}

				if ( (m_request_count%50000) == 0 ) {
					uint64_t current_millsecond = wawo::time::curr_milliseconds();
					uint64_t diff_time = current_millsecond - m_check_millseconds ;
					if( diff_time != 0 ) {
						double curr_qps = (m_request_count/diff_time ) * 1000.0;
						m_total += m_request_count;

						uint64_t diff_time_total = current_millsecond - m_check_millseconds_begin;
						WAWO_ASSERT( diff_time_total > 0 );
						double qps_avg = (m_total/diff_time_total) * 1000.0;

						m_request_count = 0 ;

						m_check_millseconds = current_millsecond ;
						WAWO_WARN( "[echo]current qps(every 50000):%0.lf\tavg qps(total_q/total_run_time):%0.lf\ttotal q:%lld\ttotal run time: %0.2lf\thours", curr_qps, qps_avg,m_total, ((diff_time_total*1.0)/(3600.0*1000)) );
					}
				}
			} else if( command == wawo::net::service::C_ECHO_PINGPONG ) {

				WWSP<Packet> packet_o( new Packet(256) );
				packet_o->Write<uint32_t>(command);
				packet_o->Write<uint32_t>(wawo::OK);
				packet_o->WriteLeft<ServiceIdT>(this->GetId());

				WWSP<MyMessageT> message( new MyMessageT( packet_o ) );
				int rt = ctx.peer->Respond( message, incoming );
				(void) rt;
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			} else {
				WAWO_THROW_EXCEPTION("invalid echo command");
			}
		}

		virtual void OnRespond( PeerMessageCtx const& ctx, WWSP<MyMessageT> const& incoming  ) {
			(void)ctx;
			(void)incoming;
		}

	private:
		uint64_t m_total;
		std::atomic<uint32_t> m_request_count;
		uint64_t m_check_millseconds;
		uint64_t m_check_millseconds_begin;
	};

}}}

#endif