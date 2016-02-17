#ifndef _WAWO_NET_PROTOCOL_ABSTRACT_HPP
#define _WAWO_NET_PROTOCOL_ABSTRACT_HPP

#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/Packet.hpp>
#include <wawo/algorithm/BytesRingBuffer.hpp>

namespace wawo { namespace net {

	using namespace wawo::algorithm;

	class Protocol_Abstract {
	public:
		Protocol_Abstract() {}
		virtual ~Protocol_Abstract() {}

		//each type of connection must implement the following two func
		virtual uint32_t Encode( WAWO_SHARED_PTR<Packet> const& packet_in, WAWO_SHARED_PTR<Packet>& packet_out ) = 0;

		//return received packet count, set ec code if any
		virtual uint32_t DecodePackets( BytesRingBuffer* const buffer, WAWO_SHARED_PTR<Packet> packets[], uint32_t const& size, int& ec_o ) = 0 ;
	};

}}
#endif