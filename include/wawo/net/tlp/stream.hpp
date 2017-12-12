#ifndef _WAWO_NET_TLP_STREAM_PACKET_HPP_
#define _WAWO_NET_TLP_STREAM_PACKET_HPP_

#include <wawo/packet.hpp>
#include <wawo/bytes_ringbuffer.hpp>

#include <wawo/net/tlp_abstract.hpp>

#define WAWO_STREAM_PACKET_ENABLE_SPLIT
#define WAWO_STREAM_PIECE_MAX				(1024*128)

namespace wawo { namespace net { namespace tlp {

	class stream :
		public wawo::net::tlp_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(stream)

	public:
		stream() :tlp_abstract() {}
		virtual ~stream() {}

	public:
		int encode( WWSP<packet> const& in, WWSP<packet>& out ) {
			WWSP<packet> _o_ = wawo::make_shared<packet>( *in ) ;
			out.swap(_o_);
			return wawo::OK;
		}

		u32_t decode_packets(WWRP<bytes_ringbuffer> const& buffer, WWSP<packet> arrives[], u32_t const& size, int& ec_o, WWSP<packet>& out ) {
			ec_o = wawo::OK;
			if( buffer->count() == 0 ) {return 0;}

			#ifdef WAWO_STREAM_PACKET_ENABLE_SPLIT
				u32_t idx = 0;
				u32_t cbc = buffer->count();
				do {
					u32_t ps = (cbc>WAWO_STREAM_PIECE_MAX) ? WAWO_STREAM_PIECE_MAX: cbc;
					WWSP<packet> _packet = wawo::make_shared<packet>(ps) ;

					u32_t brc = buffer->read( _packet->begin(), ps );
					WAWO_ASSERT( brc <= ps );
					_packet->forward_write_index(brc);

					WAWO_ASSERT( idx<size );
					arrives[idx++] = _packet;
				} while( (buffer->count()>0) && idx<size ) ;

				if( buffer->count()>0 ) {
					ec_o = wawo::E_TLP_TRY_AGAIN;
				}

				(void) out;
				return idx;
			#else
				u32_t bc = buffer->count();
				WWSP<packet> _packet = wawo::make_shared<packet>(bc) ;

				u32_t rc = buffer->read( _packet->begin(), bc );
				WAWO_ASSERT( rc == bc );
				_packet->MoveForwardRightIndex(rc);

				WAWO_ASSERT( 0<size );
				packets[0] = _packet;
				return 1;
			#endif
		}
	};
}}}
#endif
