#ifndef _WAWO_NET_TLP_HLEN_PACKET_HPP_
#define _WAWO_NET_TLP_HLEN_PACKET_HPP_

#include <wawo/packet.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/tlp_abstract.hpp>

#ifndef WAWO_PACKET_ADD_PREFIX_SYMBOL
	//default OFF
	//#define WAWO_PACKET_ADD_PREFIX_SYMBOL	1
#endif

#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
	#define WAWO_PACKET_PREFIX_SYMBOL_FIRST		'^'
	#define WAWO_PACKET_PREFIX_SYMBOL			"^^^^"
	#define WAWO_PACKET_PREFIX_SYMBOL_SIZE		2
	#define WAWO_PACKET_PREFIX_SZ_SIZE			((WAWO_PACKET_PREFIX_SYMBOL_SIZE)+1)
#endif

namespace wawo { namespace  net { namespace tlp {

	enum packetParseState {
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
			S_PREPARE_CHECK,
			S_CHECK_PREFIX,
#endif
			S_READ_LEN,
			S_READ_CONTENT
	};

	class hlen_packet:
		public wawo::net::tlp_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(hlen_packet)

		enum NetHeaderlengthgthConfig {
			L_PREFIX_MAX	= 4, //^^^^
			L_BODY			= 4, //u32_t len of body to send via net
		};

	private:
		packetParseState m_parse_state;

#if defined(WAWO_PACKET_ADD_PREFIX_SYMBOL) && WAWO_PACKET_ADD_PREFIX_SYMBOL
		byte_t m_prefix[WAWO_PACKET_PREFIX_SZ_SIZE]; //used for packet prefix check
		u32_t m_prefix_checked_i;
#endif
		u32_t m_size;
		public:
			hlen_packet():
#if defined(WAWO_PACKET_ADD_PREFIX_SYMBOL) && WAWO_PACKET_ADD_PREFIX_SYMBOL
				m_parse_state(S_PREPARE_CHECK)
				,m_prefix_checked_i(0)
#else
				m_parse_state(S_READ_LEN)
#endif
			{
			}

			virtual ~hlen_packet() {}

			int encode( WWSP<packet> const& in, WWSP<packet>& out ) {

				if( in->len() == 0 ) {
					WAWO_ERR("[hlen_packet]empty packet in" );
					//could not send empty packet
					return wawo::E_SOCKET_EMPTY_PACKET;
				}

				WWSP<packet> _o_ = wawo::make_shared<packet>( *in );
				_o_->write_left<u32_t>( in->len() );

				//append prefix
#if defined(WAWO_PACKET_ADD_PREFIX_SYMBOL) && WAWO_PACKET_ADD_PREFIX_SYMBOL
				WAWO_ASSERT(_o_->left_capacity() >= WAWO_PACKET_PREFIX_SYMBOL_SIZE );
				_o_->write_left((byte_t*) WAWO_PACKET_PREFIX_SYMBOL,WAWO_PACKET_PREFIX_SYMBOL_SIZE);
#endif
				out = _o_;
				return wawo::OK;
			}

			//return received packet count, set ec code if any
			u32_t decode_packets(WWRP<bytes_ringbuffer> const& buffer, WWSP<packet> arrives[], u32_t const& size, int& ec_o, WWSP<packet>& out ) {
				WAWO_ASSERT( buffer != NULL );

				ec_o = wawo::OK ;
				u32_t idx = 0;

				bool bExit = false;
				do {
					switch( m_parse_state ) {
#if defined(WAWO_PACKET_ADD_PREFIX_SYMBOL) && WAWO_PACKET_ADD_PREFIX_SYMBOL
					case S_PREPARE_CHECK:
						{
							m_prefix_checked_i = 0;
							m_parse_state = S_CHECK_PREFIX;
						}
						break;
					case S_CHECK_PREFIX:
						{
							//byte_t tmp_buffer[WAWO_PACKET_PREFIX_SYMBOL_SIZE] = {0};

							u32_t to_read = (WAWO_PACKET_PREFIX_SYMBOL_SIZE - m_prefix_checked_i) ;
							bool read_ok = buffer->try_read( m_prefix , to_read );

							if( !read_ok ) {
								bExist = true;
								break;
							}

							if( wawo::strncmp( (char*)&m_prefix[0], WAWO_PACKET_PREFIX_SYMBOL, to_read ) != 0 ) {

								m_prefix_checked_i = 0;
								for( int i=(to_read-1);i>0;i--) {
									if( m_prefix[i] != WAWO_PACKET_PREFIX_SYMBOL_FIRST ) {
										*(m_prefix+i+1) = 0;//for '0'
										WAWO_WARN( "[hlen_packet]parse packet prefix failed, skip: %s", m_prefix ) ;
										WAWO_ASSERT( !"parse packet prefix failed" );

										bExist = true;
										break;
									} else {
										++m_prefix_checked_i;
									}
								}
							} else {
								m_parse_state = S_READ_HEADER;
							}
						}
						break;
#endif
					case S_READ_LEN:
						{
							bool read_ok = buffer->try_read<u32_t>( m_size );

							if( !read_ok ) {
								bExit = true;
								break;
							} else {
								WAWO_ASSERT( m_size>0 );
								if( m_size > buffer->capacity() ) {
									ec_o = wawo::E_TLP_SOCKET_BROKEN;
									WAWO_WARN( "[hlen_packet]invalid packet size: %d", m_size ) ;
									bExit = true;
									break;
								} else {
									m_parse_state = S_READ_CONTENT;
								}
							}
						}
						break;
					case S_READ_CONTENT:
						{
							WAWO_ASSERT( m_size > 0 );
							if( (m_size==0) ) {
								bExit = true;
								ec_o = wawo::E_TLP_INVALID_PACKET;
								break;
							}
							if( buffer->count() < m_size ) {
								bExit = true;
								break;
							}
							if( (idx==size) ) {
								bExit = true;
								ec_o = wawo::E_TLP_TRY_AGAIN;
								break;
							}

							WWSP<packet> pack = wawo::make_shared<packet>( m_size );
							u32_t nbytes = buffer->read(pack->begin() , m_size );
							WAWO_ASSERT( nbytes == m_size );
							pack->forward_write_index( m_size );

							//WAWO_DEBUG( "[hlen_packet]new pack created, packet size: %d", m_size );
							m_parse_state = S_READ_LEN ;

							WAWO_ASSERT(idx<size);
							arrives[idx++] = pack ;

							(void) nbytes;
						}
						break ;
					}
				} while( !bExit) ;

				(void)out;
				return idx ;
			}
		};
}}}
#endif
