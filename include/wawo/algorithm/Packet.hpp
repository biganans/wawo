#ifndef _WAWO_ALGORITHM_PACKET_HPP_
#define _WAWO_ALGORITHM_PACKET_HPP_

//#include <string>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/bytes_helper.hpp>

#define PACK_MALLOC_H_RESERVER  WAWO_MALLOC_H_RESERVE
//( left part )
#define PACK_MIN_LEFT_CAPACITY (128)
#define PACK_MAX_LEFT_CAPACITY (1024*1024*32)
#define PACK_MIN_RIGHT_CAPACITY (128)
#define PACK_MAX_RIGHT_CAPACITY (1024*1024*32)
#define PACK_MIN_CAPACITY (PACK_MIN_LEFT_CAPACITY + PACK_MIN_RIGHT_CAPACITY)
#define PACK_MAX_CAPACITY (PACK_MAX_LEFT_CAPACITY + PACK_MAX_RIGHT_CAPACITY)
#define PACK_INIT_LEFT_CAPACITY ((PACK_MIN_LEFT_CAPACITY))
#define PACK_INIT_RIGHT_CAPACITY (((1024*4)-PACK_MIN_LEFT_CAPACITY)-PACK_MALLOC_H_RESERVER)
#define PACK_INCREMENT_LEFT_SIZE ((1024*8)-PACK_MALLOC_H_RESERVER)
#define PACK_INCREMENT_RIGHT_SIZE ((1024*8)-PACK_MALLOC_H_RESERVER)

namespace wawo { namespace algorithm {

	// memory bytes pane

	// [--writeable----bytes---bytes-----begin ------bytes--------bytes----bytes--end---writeable--]

	/*
	 * 1, front, back writeable
	 * 2, read forward only, from front -> back
	 * 3,
	 */

	//len + plain bytes
	//This is a big-endian based Packet

	class Packet {

	public:
		explicit Packet( u32_t const& right_capacity = PACK_INIT_RIGHT_CAPACITY , u32_t const& left_capacity = PACK_INIT_LEFT_CAPACITY ) :
			m_buffer( NULL ),
			m_left_capacity(left_capacity),
			m_read_idx(0),
			m_right_capacity(right_capacity),
			m_write_idx(0)
		{
			_InitBuffer( left_capacity, right_capacity );
		}

		~Packet() {
			free( m_buffer );
		}

		Packet( Packet const& copy ):
			m_buffer( NULL ),
			m_left_capacity(0),
			m_read_idx(0),
			m_right_capacity(0),
			m_write_idx(0)
		 {

			m_left_capacity = copy.m_left_capacity ;
			m_right_capacity = copy.m_right_capacity;

			if( copy.Capacity() != 0 ) {

				WAWO_ASSERT( copy.m_buffer != NULL );
				m_buffer = (byte_t*) malloc( sizeof(byte_t) * copy.Capacity() );
				WAWO_NULL_POINT_CHECK(m_buffer);
		#ifdef _DEBUG
				memset(m_buffer, 'w', copy.Capacity() );
		#endif
				memcpy( m_buffer, copy.m_buffer, copy.Capacity() );
			} else {
				m_buffer = NULL;
			}

			m_read_idx = copy.m_read_idx;
			m_write_idx = copy.m_write_idx ;
		}

		Packet& operator = ( Packet const& other) {
			WAWO_ASSERT( this != &other );
			Packet(other).Swap( *this );
			return *this;
		}

		void Swap( Packet& other ) {
			wawo::swap( m_buffer, other.m_buffer );
			wawo::swap( m_left_capacity, other.m_left_capacity );
			wawo::swap( m_read_idx, other.m_read_idx );
			wawo::swap( m_right_capacity, other.m_right_capacity );
			wawo::swap( m_write_idx, other.m_write_idx );
		}

		inline u32_t Capacity() const {
			return m_left_capacity + m_right_capacity;
		}

		inline byte_t* Begin() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_read_idx;
		}

		inline byte_t* End() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_write_idx;
		}

		inline wawo::u32_t Length() const {
			WAWO_ASSERT( m_write_idx >= m_read_idx );
			return m_write_idx - m_read_idx ;
		}

		inline void MoveRight( u32_t const& bytes ) {
			WAWO_ASSERT( m_buffer != NULL );
			WAWO_CONDITION_CHECK( (Capacity()-m_write_idx) >= bytes );
			m_write_idx += bytes;
		}

		inline void Reset() {
			WAWO_ASSERT( m_left_capacity > 0 );
			WAWO_ASSERT( m_left_capacity < PACK_MAX_LEFT_CAPACITY );

			m_read_idx = m_write_idx = m_left_capacity ;
		}

		Packet* WriteLeft( byte_t const* buffer, u32_t const& len ) {

			while ( len > (_AvailableLeftCapacity()) ) {
				_ExtendLeftBufferCapacity__();
			}

			m_read_idx -= len ;
			memcpy( m_buffer + m_read_idx, buffer, len ) ;
			return this ;
		}

		//would result in memmove if left space is not enough
		template <class T>
		Packet* WriteLeft(T const& t) {
			u32_t to_write_len = sizeof(T) ;
			while ( to_write_len > (_AvailableLeftCapacity()) ) {
				_ExtendLeftBufferCapacity__();
			}

			m_read_idx -= sizeof(T);
			u32_t wnbytes = wawo::algorithm::bytes_helper::write_impl(t, (m_buffer + m_read_idx) );
			WAWO_ASSERT( wnbytes == sizeof(T) );

			(void)wnbytes;
			return this;
		}

		template <class T>
		Packet* Write(T const& t) {
			u32_t to_write_len = sizeof(T) ;
			while ( to_write_len > (_AvailableRightCapacity()) ) {
				_ExtendRightBufferCapacity__();
			}

			m_write_idx += wawo::algorithm::bytes_helper::write_impl(t, (m_buffer + m_write_idx) );
			return this;
		}

		Packet* Write( byte_t const* const buffer, u32_t const& len ) {

			while ( len > (_AvailableRightCapacity()) ) {
				_ExtendRightBufferCapacity__();
			}

			memcpy( m_buffer + m_write_idx, buffer, len ) ;
			m_write_idx += len ;

			return this ;
		}

		template <class T>
		T Read() {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= Length() );

			T t = wawo::algorithm::bytes_helper::read_impl( m_buffer + m_read_idx, wawo::algorithm::bytes_helper::type<T>() );
			m_read_idx += sizeof(T);
			return t;
		}

		u32_t Read( byte_t* const target, u32_t const& len ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( len <= Length() && len > 0 );
			WAWO_ASSERT(target != NULL );

			memcpy(target, m_buffer+m_read_idx, len);
			m_read_idx += len;
			return len;
		}

		inline void Skip( u32_t const& len  ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( len <= Length() );
			m_read_idx += len;
		}

		template <class T>
		T Look() const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= Length() );

			T t = wawo::algorithm::bytes_helper::read_impl( m_buffer + m_read_idx, wawo::algorithm::bytes_helper::type<T>() );
			return t;
		}

		u32_t Look( byte_t* const target, u32_t const& len ) const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT(len <= Length() && len > 0);
			WAWO_ASSERT(target != NULL);

			memcpy(target, m_buffer+m_read_idx, len);
			return len;
		}

	private:
		byte_t* m_buffer ;

		u32_t	m_left_capacity;
		u32_t	m_read_idx; //read index

		u32_t	m_right_capacity; //the total buffer size
		u32_t	m_write_idx; //write index

	private:
		inline u32_t _AvailableLeftCapacity() { return (m_buffer==NULL) ? 0 : m_read_idx; }
		inline u32_t _AvailableRightCapacity() { return (m_buffer==NULL) ? 0 : Capacity() - m_write_idx; }

	protected:
		void _ExtendLeftBufferCapacity__() {

			WAWO_ASSERT( m_buffer != NULL);
			WAWO_CONDITION_CHECK ( ((Capacity() + PACK_INCREMENT_LEFT_SIZE) <= PACK_MAX_LEFT_CAPACITY) );
			m_left_capacity += PACK_INCREMENT_LEFT_SIZE;
			byte_t* _newbuffer = (byte_t*) realloc(m_buffer, Capacity() );
			WAWO_NULL_POINT_CHECK( _newbuffer );
			m_buffer = _newbuffer;

			int length = Length();
			u32_t new_left = m_read_idx + PACK_INCREMENT_LEFT_SIZE ;

			if( (new_left != m_read_idx) && length > 0 ) {
				memmove( m_buffer+new_left, m_buffer + m_read_idx, length );
				m_write_idx += PACK_INCREMENT_LEFT_SIZE;
				m_read_idx = new_left;
			}
		}

		void _ExtendRightBufferCapacity__() {
			WAWO_CONDITION_CHECK( Capacity() != 0 );

			WAWO_ASSERT( m_buffer != NULL);
			WAWO_CONDITION_CHECK ( ((Capacity() + PACK_INCREMENT_RIGHT_SIZE) <= PACK_MAX_RIGHT_CAPACITY) );
			m_right_capacity += PACK_INCREMENT_RIGHT_SIZE;
			byte_t* _newbuffer = (byte_t*) realloc(m_buffer, Capacity() );
			WAWO_NULL_POINT_CHECK( _newbuffer );
			m_buffer = _newbuffer;

			WAWO_NULL_POINT_CHECK( m_buffer );
		}

		void _InitBuffer( u32_t const& left, u32_t const& right )  {

			m_left_capacity = WAWO_MAX2(left,PACK_MIN_LEFT_CAPACITY);
			m_right_capacity = WAWO_MAX2(right,PACK_MIN_RIGHT_CAPACITY);

			Reset();

			WAWO_CONDITION_CHECK( m_left_capacity <= PACK_MAX_LEFT_CAPACITY ) ;
			WAWO_CONDITION_CHECK( m_right_capacity <= PACK_MAX_RIGHT_CAPACITY ) ;

			WAWO_CONDITION_CHECK( Capacity() != 0 );
			m_buffer = (byte_t*) malloc( sizeof(byte_t) * Capacity() ) ;
			WAWO_NULL_POINT_CHECK( m_buffer ) ;
			#ifdef _DEBUG
			memset( m_buffer,'w', Capacity() ) ;
			#endif

			WAWO_NULL_POINT_CHECK( m_buffer );
		}

	};
}}
#endif