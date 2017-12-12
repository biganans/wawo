#ifndef _WAWO_PACKET_HPP_
#define _WAWO_PACKET_HPP_


#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/bytes_helper.hpp>

#define PACK_MALLOC_H_RESERVER  WAWO_MALLOC_H_RESERVE

#define PACK_MIN_LEFT_CAPACITY (128)
#define PACK_MAX_LEFT_CAPACITY (1024*1024*16)
#define PACK_MIN_RIGHT_CAPACITY (256)
#define PACK_MAX_RIGHT_CAPACITY (1024*1024*16)
#define PACK_MIN_CAPACITY (PACK_MIN_LEFT_CAPACITY + PACK_MIN_RIGHT_CAPACITY)
#define PACK_MAX_CAPACITY (PACK_MAX_LEFT_CAPACITY + PACK_MAX_RIGHT_CAPACITY)
#define PACK_INIT_LEFT_CAPACITY ((PACK_MIN_LEFT_CAPACITY))
#define PACK_INIT_RIGHT_CAPACITY (((1024*4)-PACK_MIN_LEFT_CAPACITY)-PACK_MALLOC_H_RESERVER)
#define PACK_INCREMENT_LEFT_SIZE ((1024*8)-PACK_MALLOC_H_RESERVER)
#define PACK_INCREMENT_RIGHT_SIZE ((1024*8)-PACK_MALLOC_H_RESERVER)

namespace wawo {

	// memory bytes layout
	// [--writeable----bytes---bytes-----begin ------bytes--------bytes----bytes--end---writeable--]

	/*
	 * 1, front, back writeable
	 * 2, read forward only
	 */

	class packet {

	private:
		byte_t* m_buffer;

		u32_t	m_left_capacity;
		u32_t	m_read_idx; //read index

		u32_t	m_right_capacity; //the total buffer size
		u32_t	m_write_idx; //write index

	private:
		inline u32_t _capacity() const {
			return m_left_capacity + m_right_capacity;
		}

		inline void _extend_leftbuffer_capacity__() {

			WAWO_ASSERT(m_buffer != NULL);
			WAWO_CONDITION_CHECK(((_capacity() + PACK_INCREMENT_LEFT_SIZE) <= PACK_MAX_LEFT_CAPACITY));
			m_left_capacity += PACK_INCREMENT_LEFT_SIZE;
			byte_t* _newbuffer = (byte_t*)::realloc(m_buffer, _capacity());
			WAWO_ALLOC_CHECK(_newbuffer, _capacity());
			m_buffer = _newbuffer;

			int _len = len();
			u32_t new_left = m_read_idx + PACK_INCREMENT_LEFT_SIZE;

			if ((new_left != m_read_idx) && _len > 0) {
				::memmove(m_buffer + new_left, m_buffer + m_read_idx, _len);
				m_write_idx += PACK_INCREMENT_LEFT_SIZE;
				m_read_idx = new_left;
			}
		}

		inline void _extend_rightbuffer_capacity__() {
			WAWO_CONDITION_CHECK(_capacity() != 0);

			WAWO_ASSERT(m_buffer != NULL);
			WAWO_CONDITION_CHECK(((_capacity() + PACK_INCREMENT_RIGHT_SIZE) <= PACK_MAX_RIGHT_CAPACITY));
			m_right_capacity += PACK_INCREMENT_RIGHT_SIZE;
			byte_t* _newbuffer = (byte_t*)::realloc(m_buffer, _capacity());
			WAWO_ALLOC_CHECK(_newbuffer, _capacity());
			m_buffer = _newbuffer;
		}

		void _init_buffer(u32_t const& left, u32_t const& right) {

			m_left_capacity = WAWO_MAX2(left, PACK_MIN_LEFT_CAPACITY);
			m_right_capacity = WAWO_MAX2(right, PACK_MIN_RIGHT_CAPACITY);

			reset();

			WAWO_CONDITION_CHECK(m_left_capacity <= PACK_MAX_LEFT_CAPACITY);
			WAWO_CONDITION_CHECK(m_right_capacity <= PACK_MAX_RIGHT_CAPACITY);

			WAWO_CONDITION_CHECK(_capacity() != 0);
			m_buffer = (byte_t*)::malloc(sizeof(byte_t) * _capacity());
			WAWO_ALLOC_CHECK(m_buffer, sizeof(byte_t) * _capacity());

#ifdef _DEBUG
			::memset(m_buffer, 'w', _capacity());
#endif
		}

	public:
		explicit packet( u32_t const& right_capacity = PACK_INIT_RIGHT_CAPACITY , u32_t const& left_capacity = PACK_INIT_LEFT_CAPACITY ) :
			m_buffer( NULL ),
			m_left_capacity(left_capacity),
			m_read_idx(0),
			m_right_capacity(right_capacity),
			m_write_idx(0)
		{
			_init_buffer( left_capacity, right_capacity );
		}

		~packet() {
			::free( m_buffer );
		}

		packet( packet const& copy ):
			m_buffer( NULL ),
			m_left_capacity(0),
			m_read_idx(0),
			m_right_capacity(0),
			m_write_idx(0)
		{

			m_left_capacity = copy.m_left_capacity ;
			m_right_capacity = copy.m_right_capacity;

			if( copy._capacity() != 0 ) {

				WAWO_ASSERT( copy.m_buffer != NULL );
				m_buffer = (byte_t*)::malloc( sizeof(byte_t) * copy._capacity() );
				WAWO_ALLOC_CHECK(m_buffer, sizeof(byte_t) * copy._capacity());

		#ifdef _DEBUG
				::memset(m_buffer, 'w', copy._capacity() );
		#endif
				::memcpy( m_buffer, copy.m_buffer, copy._capacity() );
			} else {
				m_buffer = NULL;
			}

			m_read_idx = copy.m_read_idx;
			m_write_idx = copy.m_write_idx ;
		}

		packet& operator = ( packet const& other) {
			WAWO_ASSERT( this != &other );
			packet(other).swap( *this );
			return *this;
		}

		void swap( packet& other ) {
			wawo::swap( m_buffer, other.m_buffer );
			wawo::swap( m_left_capacity, other.m_left_capacity );
			wawo::swap( m_read_idx, other.m_read_idx );
			wawo::swap( m_right_capacity, other.m_right_capacity );
			wawo::swap( m_write_idx, other.m_write_idx );
		}

		inline u32_t left_left_capacity() { return (m_buffer == NULL) ? 0 : m_read_idx; }
		inline u32_t left_right_capacity() { return (m_buffer == NULL) ? 0 : _capacity() - m_write_idx; }

		inline byte_t* begin() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_read_idx;
		}

		inline byte_t* end() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_write_idx;
		}

		inline wawo::u32_t len() const {
			WAWO_ASSERT( m_write_idx >= m_read_idx );
			return m_write_idx - m_read_idx ;
		}

		inline void forward_write_index( u32_t const& bytes ) {
			WAWO_ASSERT( m_buffer != NULL );
			WAWO_CONDITION_CHECK( (_capacity()-m_write_idx) >= bytes );
			m_write_idx += bytes;
		}

		inline void reset() {
			WAWO_ASSERT( m_left_capacity > 0 );
			WAWO_ASSERT( m_left_capacity < PACK_MAX_LEFT_CAPACITY );

			m_read_idx = m_write_idx = m_left_capacity ;
		}

		packet* write_left( byte_t const* buffer, u32_t const& length ) {

			while ( length > (left_left_capacity()) ) {
				_extend_leftbuffer_capacity__();
			}

			m_read_idx -= length ;
			::memcpy( m_buffer + m_read_idx, buffer, length ) ;
			return this ;
		}

		//would result in memmove if left space is not enough
		template <class T>
		packet* write_left(T const& t) {
			u32_t to_write_length = sizeof(T) ;
			while ( to_write_length > (left_left_capacity()) ) {
				_extend_leftbuffer_capacity__();
			}

			m_read_idx -= sizeof(T);
			u32_t wnbytes = wawo::bytes_helper::write_impl(t, (m_buffer + m_read_idx) );
			WAWO_ASSERT( wnbytes == sizeof(T) );

			(void)wnbytes;
			return this;
		}

		template <class T>
		packet* write(T const& t) {
			u32_t to_write_length = sizeof(T) ;
			while ( to_write_length > (left_right_capacity()) ) {
				_extend_rightbuffer_capacity__();
			}

			m_write_idx += wawo::bytes_helper::write_impl(t, (m_buffer + m_write_idx) );
			return this;
		}

		packet* write( byte_t const* const buffer, u32_t const& length ) {

			while ( length > (left_right_capacity()) ) {
				_extend_rightbuffer_capacity__();
			}

			::memcpy( m_buffer + m_write_idx, buffer, length ) ;
			m_write_idx += length ;

			return this ;
		}

		template <class T>
		T read() {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= len() );

			T t = wawo::bytes_helper::read_impl( m_buffer + m_read_idx, wawo::bytes_helper::type<T>() );
			m_read_idx += sizeof(T);
			return t;
		}

		u32_t read( byte_t* const target, u32_t const& length ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( length <= len() && length > 0 );
			WAWO_ASSERT(target != NULL );

			::memcpy(target, m_buffer+m_read_idx, length);
			m_read_idx += length;
			return length;
		}

		inline void skip( u32_t const& length  ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( length <= len() );
			m_read_idx += length;
		}

		template <class T>
		T peek() const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= len() );

			T t = wawo::bytes_helper::read_impl( m_buffer + m_read_idx, wawo::bytes_helper::type<T>() );
			return t;
		}

		u32_t peek( byte_t* const buffer, u32_t const& size ) const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT(buffer != NULL);

			u32_t can_peek_size = size >= len() ? len() : size;

			::memcpy(buffer, m_buffer+m_read_idx, can_peek_size);
			return can_peek_size;
		}
	};
}
#endif