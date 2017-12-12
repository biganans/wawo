#ifndef _WAWO_NET_TLP_ABSTRACT_HPP
#define _WAWO_NET_TLP_ABSTRACT_HPP

#include <wawo/smart_ptr.hpp>
#include <wawo/packet.hpp>
#include <wawo/bytes_ringbuffer.hpp>

namespace wawo { namespace net {

	//enum TLP_HandshakeState {
	//	TLP_HANDSHAKING = 1,
	//	TLP_HANDSHAKE_DONE
	//};

	/*
	 * tlp means Transport Layer protocol which SHOULD live upon a reliable transport level
	 * tlp providing encrypted or plain text deliver service for upper application layer

	 * the following tlp layer could be achieved by impl tlp_abstract
	 * TLS/SSL layer
	 * DH_SYMMETRIC layer
	 * by inherit this abstract class
	 */
	class tlp_abstract:
		public wawo::ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(tlp_abstract)
	public:
		tlp_abstract() {}
		virtual ~tlp_abstract() {}

		virtual void handshake_assign_cookie(void const* const cookie, u32_t const& size) { (void)&cookie;(void)size; }

		//return ProtocolHandshakState if no op error, otherwise return error code
		virtual int handshake_make_hello_packet(WWSP<packet>& hello) { (void)hello; return wawo::E_TLP_HANDSHAKE_DONE; };

		//return ProtocolHandshakState if no op error, otherwise return error code
		virtual int handshake_packet_arrive(WWSP<packet> const& in, WWSP<packet>& out) {
			WAWO_ASSERT(!"WHAT");
			(void)in;
			(void)out;
			return wawo::E_TLP_HANDSHAKE_DONE;
		};

		virtual bool handshake_handshaking() const { return false; }
		virtual bool handshake_done() const { return true; }

		//return ok and set packet_o if success, otherwise return error
		virtual int encode( WWSP<packet> const& in, WWSP<packet>& out ) = 0;

		//return received packet count, set ec code if any
		virtual u32_t decode_packets( WWRP<bytes_ringbuffer> const& buffer, WWSP<packet> arrives[], u32_t const& size, int& ec_o, WWSP<packet>& out ) = 0 ;
	};

}}
#endif