#ifndef _WAWO_NET_SERVICE_POOL_HPP_
#define _WAWO_NET_SERVICE_POOL_HPP_

#include <vector>
#include <wawo/config/default_config.h>
#include <wawo/algorithm/KVPool.hpp>

#include <wawo/net/core/Listener_Abstract.h>
#include <wawo/net/ServiceListener_Abstract.h>

//#include <wawo/net/core/NetMessage.h>
//#include <wawo/net/ServiceMessage.hpp>


namespace wawo { namespace net {

	/*
	template <class ServiceMessage>
	class ServicePool:
		public wawo::utils::Ref_Object_Abstract,
		public wawo::algorithm::KVPool<int, ServiceListener_Abstract<ServiceMessage>* >
	{

	public:
		int Authenticate( int service_id, WAWO_AUTO_PTR<Packet> const& authenticate_info ) {
		
			//iterator it = Find( service_id ) ;

			//if( it == End() ) {
			//	return wawo::E_NET_SERVICE_ID_NOT_AVAILABLE ;
			//}

			//return it->v->Authenticate( authenticate_info );

		}
		//typedef wawo::algorithm::KVPool<int, WAWO_AUTO_PTR<Provider_Abstract> >::iterator iterator ;
	};
	*/

}}
#endif