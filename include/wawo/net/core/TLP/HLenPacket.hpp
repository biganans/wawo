#ifndef _WAWO_NET_CORE_TLP_HLEN_PACKET_HPP_
#define _WAWO_NET_CORE_TLP_HLEN_PACKET_HPP_

#include <wawo/algorithm/Packet.hpp>
#include <wawo/log/LoggerManager.h>

#include <wawo/net/core/TLP_Abstract.hpp>

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

namespace wawo { namespace  net { namespace core { namespace TLP {

	enum PacketParseState {
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
			S_PREPARE_CHECK,
			S_CHECK_PREFIX,
#endif
			S_READ_LEN,
			S_READ_CONTENT
	};

	class HLenPacket:
		public wawo::net::core::TLP_Abstract
	{
		enum NetHeaderlengthConfig {
			L_PREFIX_MAX	= 4, //^^^^
			L_BODY			= 4, //u32_t length of body to send via net
		};

	private:
		PacketParseState m_parse_state;

#if WAWO_PACKET_ADD_PREFIX_SYMBOL
		byte_t m_prefix[WAWO_PACKET_PREFIX_SZ_SIZE]; //used for packet prefix check
		u32_t m_prefix_checked_i;
#endif
		u32_t m_size;
		public:
			HLenPacket():
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
				m_parse_state(S_PREPARE_CHECK)
				,m_prefix_checked_i(0)
#else
				m_parse_state(S_READ_LEN)
#endif
			{
			}

			virtual ~HLenPacket() {}

			int Encode( WWSP<Packet> const& in, WWSP<Packet>& out ) {

				if( in->Length() == 0 ) {
					WAWO_FATAL("[HLenPacket]empty packet in" );
					//could not send empty packet
					return wawo::E_SOCKET_SEND_PACKET_EMPTY ;
				}

				WWSP<Packet> _o_ = WWSP<Packet>( new Packet( *(in) ) );
				_o_->WriteLeft<u32_t>( in->Length() );

				//append prefix
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
				WAWO_ASSERT(_o_->LeftCapacity() >= WAWO_PACKET_PREFIX_SYMBOL_SIZE );
				_o_->WriteLeft((byte_t*) WAWO_PACKET_PREFIX_SYMBOL,WAWO_PACKET_PREFIX_SYMBOL_SIZE);
#endif
				out = _o_;
				return wawo::OK;
			}

			//return received packet count, set ec code if any
			u32_t DecodePackets( wawo::algorithm::BytesRingBuffer* const buffer, WWSP<Packet> arrives[], u32_t const& size, int& ec_o, WWSP<Packet>& out ) {
				WAWO_ASSERT( buffer != NULL );

				ec_o = wawo::OK ;
				u32_t idx = 0;

				bool bExist = false;
				do {
					switch( m_parse_state ) {
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
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
							bool read_ok = buffer->TryRead( m_prefix , to_read );

							if( !read_ok ) {
								bExist = true;
								break;
							}

							if( wawo::strncmp( (char*)&m_prefix[0], WAWO_PACKET_PREFIX_SYMBOL, to_read ) != 0 ) {

								m_prefix_checked_i = 0;
								for( int i=(to_read-1);i>0;i--) {
									if( m_prefix[i] != WAWO_PACKET_PREFIX_SYMBOL_FIRST ) {
										*(m_prefix+i+1) = 0;//for '0'
										WAWO_WARN( "[HLenPacket]parse packet prefix failed, skip: %s", m_prefix ) ;
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
							bool read_ok = buffer->TryRead<u32_t>( m_size );

							if( !read_ok ) {
								bExist = true;
								break;
							} else {
								if( m_size > buffer->Capacity() ) {
									ec_o = wawo::E_TLP_HLEN_LEN_READ_FAILED;
									WAWO_WARN( "[HLenPacket]invalid packet size: %d", m_size ) ;
									bExist = true;
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
							if( buffer->BytesCount() < m_size ) {
								bExist = true;
								break;
							}
							if( (idx==size) ) {
								bExist = true;
								ec_o = wawo::E_TLP_TRY_AGAIN;
								break;
							}

							WWSP<Packet> packet ( new Packet( m_size ) );
							u32_t nbytes = buffer->Read( packet->Begin() , m_size );
							WAWO_ASSERT( nbytes == m_size );
							packet->MoveRight( m_size );

							WAWO_DEBUG( "[HLenPacket]new pack created, packet size: %d", m_size );
							m_parse_state = S_READ_LEN ;

							WAWO_ASSERT(idx<size);
							arrives[idx++] = packet ;

							(void) nbytes;
						}
						break ;
					}
				} while( !bExist ) ;

				(void)out;
				return idx ;
			}
		};
}}}}
#endif
