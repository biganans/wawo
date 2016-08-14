#ifndef _WAWO_NET_CORE_TLP_ABSTRACT_HPP
#define _WAWO_NET_CORE_TLP_ABSTRACT_HPP

#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/Packet.hpp>
#include <wawo/algorithm/BytesRingBuffer.hpp>

namespace wawo { namespace net { namespace core {

	using namespace wawo::algorithm;

	enum TLP_HandshakeState {
		TLP_HANDSHAKING = 1,
		TLP_HANDSHAKE_DONE
	};

	/*
	 * TLP means Transport Layer Protocol which SHOULD live upon a reliable transport level
	 * TLP providing encrypted or plain text deliver service for upper application layer

	 * the following TLP layer could be achieved by impl TLP_Abstract
	 * TLS/SSL layer
	 * DH_SYMMETRIC layer
	 * by inherit this abstract class
	 */
	class TLP_Abstract:
		public wawo::RefObject_Abstract
	{

	public:
		TLP_Abstract() {}
		virtual ~TLP_Abstract() {}

		virtual void Handshake_AssignCookie(void const* const cookie, u32_t const& size) { (void)&cookie;(void)size; }

		//return ProtocolHandshakState if no op error, otherwise return error code
		virtual int Handshake_MakeHelloPacket(WWSP<Packet>& hello) { (void)hello; return wawo::net::core::TLP_HANDSHAKE_DONE; };

		//return ProtocolHandshakState if no op error, otherwise return error code
		virtual int Handshake_PacketArrive(WWSP<Packet> const& packet, WWSP<Packet>& out) {
			WAWO_THROW_EXCEPTION("WHAT");
			(void)packet;
			(void)out;
			return wawo::net::core::TLP_HANDSHAKE_DONE;
		};

		virtual bool Handshake_IsHandshaking() const { return false; }
		virtual bool Handshake_IsDone() const { return true; }

		//return ok and set packet_o if success, otherwise return error
		virtual int Encode( WWSP<Packet> const& in, WWSP<Packet>& out ) = 0;

		//return received packet count, set ec code if any
		virtual u32_t DecodePackets( BytesRingBuffer* const buffer, WWSP<Packet> arrives[], u32_t const& size, int& ec_o, WWSP<Packet>& out ) = 0 ;
	};

}}}
#endif