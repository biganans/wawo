#ifndef _WAWO_ALGORITHM_BYTES_RING_BUFFER_HPP_
#define _WAWO_ALGORITHM_BYTES_RING_BUFFER_HPP_

#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/bytes_helper.hpp>

namespace wawo { namespace algorithm {

	class BytesRingBuffer :
		public wawo::RefObject_Abstract {

		enum SizeRange {
			MAX_RING_BUFFER_SIZE = 1024*1024*32,
			MIN_RING_BUFFER_SIZE = 1024*2
		};
	public:
		BytesRingBuffer( wawo::u32_t const& capacity ) :
			m_capacity(capacity+1),
			m_begin(0), //read++
			m_end(0), //write++
			m_buffer(NULL)
		{
			Reset();
		}
		~BytesRingBuffer() {
			WAWO_ASSERT(m_buffer != NULL);
			free( m_buffer );
		}
		inline bool IsEmpty() const {
			return m_begin == m_end;
		}
		inline void Reset() {
			m_begin = m_end = 0;

			if( m_buffer == NULL ) {
				WAWO_ASSERT( (m_capacity-1) >= MIN_RING_BUFFER_SIZE && (m_capacity-1) <= MAX_RING_BUFFER_SIZE );
				m_buffer = (byte_t* ) malloc( (m_capacity)* sizeof(byte_t) ) ;
				WAWO_NULL_POINT_CHECK( m_buffer ) ;
#ifdef _DEBUG
				memset( m_buffer, 'd', m_capacity );
#endif
			}
		}

		inline bool IsFull() const {
			return (m_end+1) % m_capacity == m_begin;
		}

		inline wawo::u32_t Capacity() const {
			return m_capacity - 1;
		}
		inline wawo::u32_t LeftCapacity() const {
			return ( Capacity() - BytesCount() ) ;
		}

		inline wawo::u32_t BytesCount() const {
			return ((m_end - m_begin) + m_capacity ) % m_capacity;
		}

		inline wawo::u32_t Peek( wawo::byte_t* const target, wawo::u32_t const& amount ) {
			return Read( target, amount, false );
		}

		inline bool TryRead( wawo::byte_t* const target, wawo::u32_t const& amount ) {
			if( BytesCount() >= amount ) {
				u32_t read_count = Read(target, amount );
				WAWO_ASSERT( read_count == amount );
				(void)read_count;
				return true;
			}
			return false ;
		}

		template <class T>
		inline T Read() {
			WAWO_ASSERT( m_buffer != NULL );
#ifdef _DEBUG
			byte_t tmp[sizeof(T)] = {'d'} ;
#else
			byte_t tmp[sizeof(T)] ;
#endif
			WAWO_ASSERT( BytesCount() >= sizeof(T) );

			u32_t rnbytes = Read( tmp, sizeof(T) );
			WAWO_ASSERT( rnbytes == sizeof(T) );
			(void)rnbytes;

			return wawo::algorithm::bytes_helper::read_impl( tmp, wawo::algorithm::bytes_helper::type<T>() );
		}

		template <class T>
		inline bool TryRead(T& t) {
			if( BytesCount() < sizeof(T) ) {
				return false;
			}
			return (t = Read<T>()), true ;
		}

		inline void Skip( wawo::u32_t const& s ) {

			WAWO_ASSERT( s > 0 );
			wawo::u32_t avail_c = BytesCount();
			WAWO_ASSERT( s <= avail_c );

			m_begin = ((m_begin + s) % m_capacity) ;

			if( IsEmpty() ) {
				Reset() ;
			}

			(void) avail_c;
			return ;
		}

		//return read count
		wawo::u32_t Read( wawo::byte_t* const target, wawo::u32_t const& s, bool const& move = true ) {
			wawo::u32_t avail_c = BytesCount();

			if( avail_c == 0 ) {
				return 0;
			}

			//confirm not exceed the buffer
			//try to read as much as possible, then return the read count
			wawo::u32_t copy_c = ( avail_c > s ) ? s : avail_c ;

			if( m_end > m_begin ) {
				//[------b|-----copy-bytes|----e-------]

				memcpy( target, (m_buffer+m_begin), copy_c );
		//		m_begin += should_copy_bytes_count;
			} else {

				wawo::u32_t tail_c = m_capacity - m_begin ;

				if( tail_c >= copy_c ) {
					//tail buffer is enough
					//[------e-----b|---copy-bytes--|----]

					memcpy( target, (m_buffer+m_begin ), copy_c );
//					m_begin = (m_begin + should_copy_bytes_count) % m_capacity ;

				} else {

					//[|--copy-bytes--|---e-----b|---copy-bytes----|]
					//copy tail first , then header

					memcpy( target, m_buffer+m_begin, tail_c );
//					m_begin = 0; //reset m_begin position

					wawo::u32_t head_c = copy_c - tail_c;
					memcpy( target + tail_c, m_buffer, head_c );
				}
			}

			if( move ) {
				Skip(copy_c) ;
			}

			return copy_c ;
		}

		wawo::u32_t Write(const byte_t* const bytes, wawo::u32_t const& s ) {

			WAWO_ASSERT( s != 0 );
			WAWO_ASSERT( m_buffer != NULL );

			wawo::u32_t avail_s = LeftCapacity();

			if( avail_s == 0 ) {
				return 0;
			}

			if( m_end == m_begin ) {
				m_end = 0;
				m_begin = 0;
			}

			//try to writes as much as possible, then return the write bytes count
			wawo::u32_t to_write_c = ( avail_s > s ) ? s : avail_s ;

			if( m_end <= m_begin ) {

				//[----------e|---write-bytes----|--b------]
				memcpy( m_buffer+m_end, bytes, to_write_c );
				m_end = ( m_end+to_write_c ) % m_capacity ;

			} else if( m_end > m_begin ) {
				//[------s--------e-----]

				wawo::u32_t tail_s = m_capacity - m_end;
				if( tail_s >= to_write_c ) {

					//[----------b---------e|--write-bytes--|--]
					memcpy( m_buffer+m_end, bytes, to_write_c ) ;
					m_end = (m_end+to_write_c) % m_capacity ;
				} else {

					//[|---writes-bytes---|----b---------e|--write-bytes----|]

					memcpy( m_buffer+m_end, bytes, tail_s );
					m_end = 0;

					wawo::u32_t header_write_c = to_write_c - tail_s;
					memcpy( m_buffer + m_end, bytes+tail_s, header_write_c );

					m_end = (m_end+header_write_c) % m_capacity ;
				}
			}
			return to_write_c ;
		}
	private:
		u32_t m_capacity; //total capacity
		u32_t m_begin; //read
		u32_t m_end; //write

		byte_t* m_buffer;
	};
}}
#endif //_BYTE_RING_BUFFER_
