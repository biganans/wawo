#ifndef WAWO_NET_PEER_MESSAGE_PACKET_HPP
#define WAWO_NET_PEER_MESSAGE_PACKET_HPP

#include <wawo/core.hpp>

#include <wawo/packet.hpp>

namespace wawo { namespace net { namespace peer { namespace message {

	struct cargo
	{
		WWSP<packet> data;

		explicit cargo(WWSP<packet> const& packet) :
			data(packet)
		{
		}
		~cargo()
		{}

		cargo(cargo const& other) :
			data(NULL)
		{
			if (other.data != NULL) {
				data = wawo::make_shared<packet>(*(other.data));
			}
		}

		cargo& operator=(cargo const& other) {
			cargo(other).swap(*this);
			return *this;
		}

		void swap(cargo& other) {
			data.swap(other.data);
		}

		int encode(WWSP<packet>& packet_o) {
			WAWO_ASSERT(data != NULL);
			WWSP<packet> _p = wawo::make_shared<packet>(*data);
			packet_o = _p;
			return wawo::OK;
		}
	};

}}}}

#endif