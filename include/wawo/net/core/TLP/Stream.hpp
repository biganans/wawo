#ifndef _WAWO_NET_CORE_TLP_STREAM_PACKET_HPP_
#define _WAWO_NET_CORE_TLP_STREAM_PACKET_HPP_

#include <wawo/algorithm/Packet.hpp>
#include <wawo/algorithm/BytesRingBuffer.hpp>

#include <wawo/net/core/TLP_Abstract.hpp>

#define WAWO_STREAM_PACKET_ENABLE_SPLIT
#define WAWO_STREAM_PIECE_MAX				(1024*128)

namespace wawo { namespace net { namespace core { namespace TLP {

	using namespace wawo::algorithm;

	class Stream :
		public wawo::net::core::TLP_Abstract
	{

	public:
		int Encode( WWSP<Packet> const& in, WWSP<Packet>& out ) {
			WWSP<Packet> _o_ ( new Packet( *in ) ) ;
			out.swap(_o_);
			return wawo::OK;
		}

		u32_t DecodePackets(BytesRingBuffer* const buffer, WWSP<Packet> arrives[], u32_t const& size, int& ec_o, WWSP<Packet>& out ) {
			ec_o = wawo::OK;
			if( buffer->BytesCount() == 0 ) {return 0;}

			#ifdef WAWO_STREAM_PACKET_ENABLE_SPLIT
				u32_t idx = 0;
				u32_t cbc = buffer->BytesCount();
				do {
					u32_t ps = (cbc>WAWO_STREAM_PIECE_MAX) ? WAWO_STREAM_PIECE_MAX: cbc;
					WWSP<Packet> _packet( new Packet(ps) );

					u32_t brc = buffer->Read( _packet->Begin(), ps );
					WAWO_ASSERT( brc <= ps );
					_packet->MoveRight(brc);

					WAWO_ASSERT( idx<size );
					arrives[idx++] = _packet;
				} while( (buffer->BytesCount()>0) && idx<size ) ;

				if( buffer->BytesCount()>0 ) {
					ec_o = wawo::E_TLP_TRY_AGAIN;
				}

				(void) out;
				return idx;
			#else
				u32_t bc = buffer->BytesCount();
				WWSP<Packet> _packet( new Packet(bc) );

				u32_t rc = buffer->Read( _packet->Begin(), bc );
				WAWO_ASSERT( rc == bc );
				_packet->MoveForwardRightIndex(rc);

				WAWO_ASSERT( 0<size );
				packets[0] = _packet;
				return 1;
			#endif
		}
	};
}}}}
#endif
