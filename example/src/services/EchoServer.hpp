#ifndef _SERVICES_ECHO_SERVER_H_
#define _SERVICES_ECHO_SERVER_H_

#include <atomic>

#include <wawo.h>
#include "ServiceCommand.h"

namespace services {

	using namespace wawo::net;

	template <class _MyPeerT>
	class EchoServer:
		public wawo::net::ServiceProvider_Abstract<_MyPeerT>
	{
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::MessageT MyMessageT;

	public:
		EchoServer( wawo::u32_t id ):
			ServiceProvider_Abstract<_MyPeerT>(id),
			m_total(0),
			m_request_count(0),
			m_check_millseconds( wawo::time::curr_milliseconds() ),
			m_check_millseconds_begin(wawo::time::curr_milliseconds())
		{
		}
		~EchoServer() {}

		virtual void WaitMessage(WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {

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

		virtual void OnReceive( WWRP<MyPeerT> peer, WWSP<MyMessageT> const& incoming ){
			(void)peer;
			(void)incoming;
		}

		virtual void OnRequest(WWRP<MyPeerT> peer, WWSP<MyMessageT> const& incoming ) {

			WWSP<Packet> const& packet = incoming->GetPacket();

			wawo::u32_t command = packet->Read<wawo::u32_t>();

			if( command == services::C_ECHO_HELLO ||
				command == services::C_ECHO_STRING_REQUEST_TEST ||
				command == services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES
			) {
				wawo::u32_t hello_len = packet->Read<wawo::u32_t>();
				wawo::byte_t* hello_string = NULL;

				if (hello_len > 0) {
					hello_string = (wawo::byte_t*)malloc(hello_len * sizeof(wawo::byte_t));
					packet->Read(hello_string, hello_len);
				}

				WWSP<Packet> packet_o( new Packet(256) );
				packet_o->Write<wawo::u32_t>( command );
				packet_o->Write<wawo::u32_t>(wawo::OK);
				packet_o->Write<wawo::u32_t>(hello_len);

				if (hello_len > 0) {
					packet_o->Write(hello_string, hello_len);
					free(hello_string);
				}

				packet_o->WriteLeft<ServiceIdT>(this->GetId());
				WWSP<MyMessageT> message( new MyMessageT( packet_o ) );
				int rt = peer->Respond( message, incoming );


				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);

				if( rt == wawo::OK ) {
					m_request_count ++ ;
				}

				if ( (m_request_count%50000) == 0 ) {
					wawo::u64_t current_millsecond = wawo::time::curr_milliseconds();
					wawo::u64_t diff_time = current_millsecond - m_check_millseconds ;
					if( diff_time != 0 ) {
						double curr_qps = (m_request_count/diff_time ) * 1000.0;
						m_total += m_request_count;

						wawo::u64_t diff_time_total = current_millsecond - m_check_millseconds_begin;
						WAWO_ASSERT( diff_time_total > 0 );
						double qps_avg = (m_total/diff_time_total) * 1000.0;

						m_request_count = 0 ;

						m_check_millseconds = current_millsecond ;
						WAWO_WARN( "[echo]current qps(every 50000):%0.lf\tavg qps(total_q/total_run_time):%0.lf\ttotal q:%lld\ttotal run time: %0.2lf\thours", curr_qps, qps_avg,m_total, ((diff_time_total*1.0)/(3600.0*1000)) );
					}
				}
			} else if( command == services::C_ECHO_PINGPONG ) {

				WWSP<Packet> packet_o( new Packet(256) );
				packet_o->Write<wawo::u32_t>(command);
				packet_o->Write<wawo::u32_t>(wawo::OK);
				packet_o->WriteLeft<ServiceIdT>(this->GetId());

				WWSP<MyMessageT> message( new MyMessageT( packet_o ) );
				int rt = peer->Respond( message, incoming );
				(void) rt;
				WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			} else {
				WAWO_THROW_EXCEPTION("invalid echo command");
			}
		}

		virtual void OnRespond(WWRP<MyPeerT> peer, WWSP<MyMessageT> const& incoming  ) {
			(void)peer;
			(void)incoming;
		}

	private:
		wawo::u64_t m_total;
		std::atomic<wawo::u32_t> m_request_count;
		wawo::u64_t m_check_millseconds;
		wawo::u64_t m_check_millseconds_begin;
	};

}

#endif