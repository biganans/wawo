#ifndef _SERVICES_ECHO_SERVER_H_
#define _SERVICES_ECHO_SERVER_H_

#include <atomic>

#include <wawo.h>
#include "ServiceShare.h"

namespace services {

	using namespace wawo;
	using namespace wawo::net;

	template <class _MyPeerT>
	class EchoServer:
		public wawo::net::service_provider_abstract<_MyPeerT>
	{
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::message_t MyMessageT;

	public:
		EchoServer( wawo::u32_t id ):
			service_provider_abstract<_MyPeerT>(id),
			m_total(0),
			m_request_count(0),
			m_check_millseconds( wawo::time::curr_milliseconds() ),
			m_check_millseconds_begin(wawo::time::curr_milliseconds())
		{
		}
		~EchoServer() {}

		virtual void wait_message(WWRP<MyPeerT> const& peer, WWSP<MyMessageT> const& incoming ) {

			int type = incoming->type;
			switch( type ) {
			case wawo::net::peer::message::ros::T_SEND:
				{
					OnReceive(peer, incoming );
				}
				break;
			case wawo::net::peer::message::ros::T_REQUEST:
				{
					OnRequest(peer, incoming );
				}
				break;
			}
		}

		virtual void OnReceive( WWRP<MyPeerT> peer, WWSP<MyMessageT> const& incoming ){
			(void)peer;
			(void)incoming;
		}

		virtual void OnRequest(WWRP<MyPeerT> peer, WWSP<MyMessageT> const& incoming ) {

			WWSP<packet> const& inpack = incoming->data;

			wawo::u32_t command = inpack->read<wawo::u32_t>();

			if( command == services::C_ECHO_HELLO ||
				command == services::C_ECHO_STRING_REQUEST_TEST ||
				command == services::C_ECHO_STRING_REQUEST_TEST_JUST_PRESSES
			) {
				wawo::u32_t hello_len = inpack->read<wawo::u32_t>();
				wawo::byte_t* hello_string = NULL;

				if (hello_len > 0) {
					hello_string = (wawo::byte_t*)malloc(hello_len * sizeof(wawo::byte_t));
					inpack->read(hello_string, hello_len);
				}

				WWSP<packet> packet_o = wawo::make_shared<packet>(256) ;
				packet_o->write<wawo::u32_t>( command );
				packet_o->write<wawo::u32_t>(wawo::OK);
				packet_o->write<wawo::u32_t>(hello_len);

				if (hello_len > 0) {
					packet_o->write(hello_string, hello_len);
					free(hello_string);
				}

				//packet_o->WriteLeft<ServiceIdT>(this->GetId());
				WWSP<MyMessageT> message = wawo::make_shared<MyMessageT>( packet_o );
				int rt = peer->respond( message, incoming );

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

				WWSP<packet> packet_o = wawo::make_shared<packet>(256) ;
				packet_o->write<wawo::u32_t>(command);
				packet_o->write<wawo::u32_t>(wawo::OK);
				packet_o->write_left<service_id_t>(this->get_id());

				WWSP<MyMessageT> message = wawo::make_shared<MyMessageT>( packet_o );
				int rt = peer->respond( message, incoming );

				(void) rt;
				//WAWO_CHECK_SOCKET_SEND_RETURN_V(rt);
			} else {
				WAWO_THROW_EXCEPTION("invalid echo command");
			}
		}

	private:
		wawo::u64_t m_total;
		std::atomic<wawo::u32_t> m_request_count;
		wawo::u64_t m_check_millseconds;
		wawo::u64_t m_check_millseconds_begin;
	};

}

#endif