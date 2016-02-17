#ifndef _WAWO_NET_MESSAGE_ABSTRACT_HPP
#define _WAWO_NET_MESSAGE_ABSTRACT_HPP

#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/Packet.hpp>

namespace wawo { namespace net {

	/*

	just impl the following two method in your message class
	class Message_Abstract {
	public:
		static int Encode( WAWO_SHARED_PTR<YouMessageT> const& message,  WAWO_SHARED_PTR<Packet>& packet ) ;
		static uint32_t Decode( WAWO_SHARED_PTR<Packet> const& packet, WAWO_SHARED_PTR<YouMessageT> messages[], uint32_t const& size, int& ec_o ) = 0;
	};

	*/
}}
#endif