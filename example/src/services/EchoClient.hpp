#ifndef _SERVICES_ECHO_CLIENT_H_
#define _SERVICES_ECHO_CLIENT_H_

#include <wawo.h>
#include "ServiceShare.h"

namespace services {

	using namespace wawo;

	template <class _Mypeer_t>
	class EchoClient:
		public wawo::net::service_provider_abstract<_Mypeer_t>
	{
		//for gcc...
		typedef _Mypeer_t Mypeer_t;
		typedef typename _Mypeer_t::message_t MyMessageT;


	public:

		EchoClient(u32_t id):
			service_provider_abstract<Mypeer_t>(id)
		{
		}

		~EchoClient() {
		}

		virtual void wait_message( WWRP<Mypeer_t> const& peer, WWSP<MyMessageT> const& incoming ) {

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

		void OnReceive(WWRP<Mypeer_t> const& peer, WWSP<MyMessageT> const& incoming ) {
		}

		void OnRequest(WWRP<Mypeer_t> const& peer, WWSP<MyMessageT> const& incoming ) {
		}

	};

}

#endif