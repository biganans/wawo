#ifndef WAWO_NET_PEER_MESSAGE_PACKET_HPP
#define WAWO_NET_PEER_MESSAGE_PACKET_HPP

#include <wawo/core.h>

#include <wawo/algorithm/Packet.hpp>

namespace wawo { namespace net { namespace peer { namespace message {

	using namespace wawo::algorithm;

	class PacketMessage
	{
	private:
		WWSP<Packet> m_packet;

	public:
		explicit PacketMessage(WWSP<Packet> const& packet) :
			m_packet(packet)
		{
		}
		~PacketMessage()
		{}

		PacketMessage(PacketMessage const& other) :
			m_packet(NULL)
		{
			if (other.m_packet != NULL) {
				m_packet = WWSP<Packet>(new Packet(*(other.m_packet)));
			}
		}

		PacketMessage& operator=(PacketMessage const& other) {
			PacketMessage(other).Swap(*this);
			return *this;
		}

		void Swap(PacketMessage& other) {
			m_packet.swap(other.m_packet);
		}

		int Encode(WWSP<Packet>& packet_o) {
			WAWO_ASSERT(m_packet != NULL);
			WWSP<Packet> _p(new Packet(*m_packet));
			packet_o = _p;
			return wawo::OK;
		}

		WWSP<Packet> GetPacket() const { return m_packet; }
		void SetPacket(WWSP<Packet> const& packet) { m_packet = packet; }
	};

}}}}

#endif