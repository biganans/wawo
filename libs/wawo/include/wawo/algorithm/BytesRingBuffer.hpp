#ifndef _WAWO_ALGORITHM_BYTES_RING_BUFFER_HPP_
#define _WAWO_ALGORITHM_BYTES_RING_BUFFER_HPP_

//#include <wawo/core.h>
#include <wawo/NonCopyable.hpp>
//#include <wawo/algorithm/bytes_helper.hpp>

#define WAWO_BYTES_RINGBUFFER_HIGH_PERFORMANCE_MODE 1

/*
 this is a optimized ring buffer for bytes write&read
*/

namespace wawo { namespace algorithm {

	class BytesRingBuffer : 
		public wawo::NonCopyable {

		enum SizeRange {
			MAX_RING_BUFFER_SIZE = 1024*1024*64,
			MIN_RING_BUFFER_SIZE = 1024*2
		};
	public:
		BytesRingBuffer( wawo::uint32_t const& space ) :
		m_space(space+1),
		m_begin(0), //read++
		m_end(0) //write++
		{

			WAWO_ASSERT( space >= MIN_RING_BUFFER_SIZE && space <= MAX_RING_BUFFER_SIZE );
			m_buffer = (byte_t* ) malloc( (m_space)* sizeof(byte_t) ) ;
			WAWO_NULL_POINT_CHECK( m_buffer ) ;

#ifdef _DEBUG
			memset( m_buffer, 'd', m_space );
#endif
		}
		~BytesRingBuffer() {
			WAWO_ASSERT(m_buffer != NULL);
			free( m_buffer );
		}
		inline bool IsEmpty() {
			return m_begin == m_end;
		}
		inline void Reset() {
			m_begin = m_end = 0;
		}

		inline bool IsFull() {
			return (m_end+1) % m_space == m_begin;
		}

		inline wawo::uint32_t TotalSpace() {
			return m_space - 1;
		}
		inline wawo::uint32_t LeftSpace() {
			return ( TotalSpace() - BytesCount() ) ;
		}

		inline wawo::uint32_t BytesCount() {
			return ((m_end - m_begin) + m_space ) % m_space;
		}

		inline wawo::uint32_t Peek( wawo::byte_t* const target, wawo::uint32_t const& amount ) {
			return Read( target, amount, false );
		}

		inline bool TryRead( wawo::byte_t* const target, wawo::uint32_t const& amount ) {
		
			if( BytesCount() >= amount ) {
				uint32_t read_count = Read(target, amount );
				WAWO_ASSERT( read_count == amount );
				return true;
			}

			return false ;
		}

		template <class T>
		inline T Read() {
#ifdef _DEBUG
			byte_t tmp[sizeof(T)] = {'d'} ;
#else
			byte_t tmp[sizeof(T)] ;
#endif
			WAWO_ASSERT( BytesCount() >= sizeof(T) );

			uint32_t c = Read( tmp, sizeof(T) );
			WAWO_ASSERT( c == sizeof(T) );
		
			return wawo::algorithm::bytes_helper::read_impl( tmp, wawo::algorithm::bytes_helper::type<T>() );
		}

		template <class T>
		inline bool TryRead(T& t) {

			if( BytesCount() < sizeof(T) ) {
				return false;
			}

			return (t = Read<T>()), true ;
		}

		inline void Skip( wawo::uint32_t const& s ) {

			WAWO_ASSERT( s > 0 );
			wawo::uint32_t avail_c = BytesCount();
			WAWO_ASSERT( s <= avail_c );

			m_begin = ((m_begin + s) % m_space) ;

#ifdef WAWO_BYTES_RINGBUFFER_HIGH_PERFORMANCE_MODE
			if( IsEmpty() ) {
				Reset() ;
			}
#endif
			return ;
		}

		//return read count
		wawo::uint32_t Read( wawo::byte_t* const target, wawo::uint32_t const& s, bool const& move = true ) {
			wawo::uint32_t avail_c = BytesCount();

			if( avail_c == 0 ) {
				return 0;
			}

			//confirm not exceed the buffer
			//try to read as much as possible, then return the read count
			wawo::uint32_t copy_c = ( avail_c > s ) ? s : avail_c ;

			if( m_end > m_begin ) {
				//[------b|-----copy-bytes|----e-------]

				memcpy( target, (m_buffer+m_begin), copy_c );
		//		m_begin += should_copy_bytes_count;
			} else {

				wawo::uint32_t tail_c = m_space - m_begin ;

				if( tail_c >= copy_c ) {
					//tail buffer is enough
					//[------e-----b|---copy-bytes--|----]

					memcpy( target, (m_buffer+m_begin ), copy_c );
//					m_begin = (m_begin + should_copy_bytes_count) % m_space ;

				} else {

					//[|--copy-bytes--|---e-----b|---copy-bytes----|]
					//copy tail first , then header

					memcpy( target, m_buffer+m_begin, tail_c );
//					m_begin = 0; //reset m_begin position

					wawo::uint32_t head_c = copy_c - tail_c;
					memcpy( target + tail_c, m_buffer, head_c );
				}
			}

			if( move ) {
				Skip(copy_c) ;
			}

			return copy_c ;
		}

		wawo::uint32_t Write(const byte_t* const bytes, wawo::uint32_t const& s ) {

			WAWO_ASSERT( s > 0 );

			if( s == 0 ) {
				return 0;
			}

			wawo::uint32_t avail_s = LeftSpace();

			if( avail_s == 0 ) {
				return 0;
			}

			if( m_end == m_begin ) {
				m_end = 0;
				m_begin = 0;
			}

			//try to writes as much as possible, then return the write bytes count
			wawo::uint32_t to_write_c = ( avail_s > s ) ? s : avail_s ;

			if( m_end <= m_begin ) {

				//[----------e|---write-bytes----|--b------]
				memcpy( m_buffer+m_end, bytes, to_write_c );
				m_end = ( m_end+to_write_c ) % m_space ;

			} else if( m_end > m_begin ) {
				//[------s--------e-----]

				wawo::uint32_t tail_s = m_space - m_end;
				if( tail_s >= to_write_c ) {

					//[----------b---------e|--write-bytes--|--]
					memcpy( m_buffer+m_end, bytes, to_write_c ) ;
					m_end = (m_end+to_write_c) % m_space ;
				} else {

					//[|---writes-bytes---|----b---------e|--write-bytes----|]

					memcpy( m_buffer+m_end, bytes, tail_s );
					m_end = 0;

					wawo::uint32_t header_write_c = to_write_c - tail_s;
					memcpy( m_buffer + m_end, bytes+tail_s, header_write_c );

					m_end = (m_end+header_write_c) % m_space ;
				}

			}

			return to_write_c ;
		}
	private:
		uint32_t m_space; //total space
		uint32_t m_begin; //read
		uint32_t m_end; //write

		byte_t* m_buffer;
	};
}}

#endif //_NET_BYTE_RING_BUFFER_