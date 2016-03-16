#ifndef _WAWO_NET_PROTOCOL_WAWO_HPP_
#define _WAWO_NET_PROTOCOL_WAWO_HPP_


#include <wawo/net/Protocol_Abstract.hpp>
#include <wawo/net/core/Socket.h>
#include <wawo/algorithm/Packet.hpp>

#ifndef WAWO_PACKET_ADD_PREFIX_SYMBOL
	#define WAWO_PACKET_ADD_PREFIX_SYMBOL	1
#endif

#define WAWO_PACKET_PREFIX_SYMBOL_FIRST		'^'
#define WAWO_PACKET_PREFIX_SYMBOL			"^^^^"
#define WAWO_PACKET_PREFIX_SYMBOL_SIZE		2
#define WAWO_PACKET_PREFIX_SZ_SIZE			((WAWO_PACKET_PREFIX_SYMBOL_SIZE)+1)

#define WAWO_MAXMIUM_PACKET_PARSE_ERROR_COUNT		3

namespace wawo { namespace net { namespace protocol {

	using namespace wawo::net::core;

	enum PacketParseState {
			S_PREPARE = 0,
			S_CHECK_PREFIX,
			S_READ_HEADER,
			S_READ_CONTENT
		} ;

	class Wawo:
		public wawo::net::Protocol_Abstract
	{
		enum NetHeaderlengthConfig {
			L_PREFIX_MAX	= 4, //^^^^
			L_BODY			= 4, //uint32_t length of body to send via net
		};

	private:
		PacketParseState m_parse_state;
		byte_t m_prefix[WAWO_PACKET_PREFIX_SZ_SIZE]; //used for packet prefix check
		uint32_t m_prefix_checked_i;
		uint32_t m_size;

		uint32_t m_parse_error_c:8; //parse error count, if exceed 3, close connection

		public:
			inline static uint32_t GetNetHeaderLMax() {
				return (L_PREFIX_MAX + L_BODY) ;
			}

			Wawo():
				m_parse_state(S_PREPARE),
				m_prefix_checked_i(0),
				m_parse_error_c(0)
			{
			}

			virtual ~Wawo() {}

			//each type of connection must implement the following two func
			uint32_t Encode( WAWO_SHARED_PTR<Packet> const& packet_in, WAWO_SHARED_PTR<Packet>& packet_out ) {

				if( packet_in->Length() == 0 ) {
					WAWO_LOG_FATAL("protocol_wawo","[%d]could not encode empty packet" );
					//could not send empty packet
					return wawo::E_SOCKET_SEND_PACKET_EMPTY ;
				}

#ifdef WAWO_ENABLE_SAFE_ENCODER
				//security first
				packet_out = WAWO_REF_PTR<Packet>( new Packet( *(packet_in.Get()) ) );
#endif

				//performance first
				packet_out = packet_in;


				//append length
				packet_out->WriteLeft<uint32_t>( packet_in->Length() );

				//append prefix
#ifdef WAWO_PACKET_ADD_PREFIX_SYMBOL
				WAWO_ASSERT( packet_out->LeftCapacity() >= WAWO_PACKET_PREFIX_SYMBOL_SIZE );
				packet_out->WriteLeft((byte_t*) WAWO_PACKET_PREFIX_SYMBOL,WAWO_PACKET_PREFIX_SYMBOL_SIZE);
#endif

				return wawo::OK;
			}

			//return received packet count, set ec code if any
			uint32_t DecodePackets( wawo::algorithm::BytesRingBuffer* const buffer, WAWO_SHARED_PTR<Packet> packets[], uint32_t const& size, int& ec_o ) {
				WAWO_ASSERT( buffer != NULL );
				WAWO_ASSERT( size >= 0 );

				bool exit_parse = false;
				ec_o = wawo::OK ;
				uint32_t count = 0;

				do {

					switch( m_parse_state ) {

					case S_PREPARE:
						{
#if WAWO_PACKET_ADD_PREFIX_SYMBOL
							m_prefix_checked_i = 0;
							m_parse_state = S_CHECK_PREFIX;
#else
							m_parse_state = S_READ_HEADER ;
#endif
						}
						break;
					case S_CHECK_PREFIX:
						{
							//byte_t tmp_buffer[WAWO_PACKET_PREFIX_SYMBOL_SIZE] = {0};

							uint32_t to_read = (WAWO_PACKET_PREFIX_SYMBOL_SIZE - m_prefix_checked_i) ;
							bool read_ok = buffer->TryRead( m_prefix , to_read );

							if( !read_ok ) {
								exit_parse = true;
								break;
							}

							if( wawo::strncmp( (char*)&m_prefix[0], WAWO_PACKET_PREFIX_SYMBOL, to_read ) != 0 ) {

								m_prefix_checked_i = 0;
								for( int i=(to_read-1);i>0;i--) {
									if( m_prefix[i] != WAWO_PACKET_PREFIX_SYMBOL_FIRST ) {
										*(m_prefix+i+1) = 0;//for '0'
										WAWO_LOG_WARN( "protocol_wawo", "parse packet prefix failed, skip: %s", m_prefix ) ;
										WAWO_ASSERT( !"parse packet prefix failed" );
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
					case S_READ_HEADER:
						{
							bool read_ok = buffer->TryRead<uint32_t>( m_size );

							if( !read_ok ) {
								exit_parse = true;
								break;
							} else {

								if( m_size > buffer->TotalSpace() ) {
									++m_parse_error_c ;
									if(m_parse_error_c>WAWO_MAXMIUM_PACKET_PARSE_ERROR_COUNT) {
										//error ,,exit
										ec_o = wawo::E_SOCKET_PROTOCOL_PARSE_PACKET_FAILED ;
										exit_parse = true;
										break;
									} else {
										WAWO_LOG_FATAL( "protocol_wawo", "received packet size large than ringbuffer size, error time: %d", m_parse_error_c );
										m_parse_state = S_PREPARE;
									}
								} else {
									m_parse_error_c = 0;
									m_parse_state = S_READ_CONTENT;
								}
							}
						}
						break;
					case S_READ_CONTENT:
						{
							WAWO_ASSERT( m_size > 0 );

							if( buffer->BytesCount() < m_size ) {
								exit_parse = true;
							} else {
								WAWO_SHARED_PTR<Packet> packet ( new Packet( m_size ) );
								uint32_t read_bytes = buffer->Read( packet->Begin() , m_size );
								WAWO_ASSERT( read_bytes == m_size );
								packet->MoveForwardRightIndex( m_size );

								WAWO_LOG_DEBUG( "protocol_wawo", "new pack created, packet size: %d", m_size );
								packets[count++]=packet ;
								m_parse_state = S_PREPARE ;

								if( (count == size) && (buffer->BytesCount()>0) ) {
									exit_parse = true;
									ec_o = wawo::E_SOCKET_MAY_HAVE_MORE_PACKET ;

									break;
								}
							}
						}
						break ;
					}
				} while( !exit_parse ) ;
			return count;
		}
	};

}}}
#endif
