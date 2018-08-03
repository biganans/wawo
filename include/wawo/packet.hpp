#ifndef _WAWO_PACKET_HPP_
#define _WAWO_PACKET_HPP_


#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/bytes_helper.hpp>

#define PACK_MALLOC_H_RESERVER  WAWO_MALLOC_H_RESERVE


#define PACK_MIN_LEFT_CAPACITY (128)
#define PACK_MAX_LEFT_CAPACITY (1024*1024*8)
#define PACK_MIN_RIGHT_CAPACITY (256)
#define PACK_MAX_RIGHT_CAPACITY (1024*1024*64)
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

	class packet : public ref_base {
		byte_t* m_buffer;

		wawo::u32_t	m_left_capacity;
		wawo::u32_t	m_right_capacity; //the total buffer size
		wawo::u32_t	m_read_idx; //read index
		wawo::u32_t	m_write_idx; //write index

	private:
		__WW_FORCE_INLINE wawo::u32_t _capacity() const {
			return m_left_capacity + m_right_capacity;
		}

		inline void _extend_leftbuffer_capacity__() {

			WAWO_ASSERT(m_buffer != NULL);
			WAWO_CONDITION_CHECK((( m_left_capacity + PACK_INCREMENT_LEFT_SIZE) <= PACK_MAX_LEFT_CAPACITY));
			m_left_capacity += PACK_INCREMENT_LEFT_SIZE;
			byte_t* _newbuffer = (byte_t*)::realloc(m_buffer, _capacity());
			WAWO_ALLOC_CHECK(_newbuffer, _capacity());
			m_buffer = _newbuffer;

			const wawo::u32_t new_left = m_read_idx + PACK_INCREMENT_LEFT_SIZE;
			wawo::u32_t _len = len();
			if (_len>0) {
				::memmove(m_buffer + new_left, m_buffer + m_read_idx, _len);
			}
			m_read_idx = new_left;
			m_write_idx = m_read_idx+_len;
		}

		inline void _extend_rightbuffer_capacity__() {
			WAWO_CONDITION_CHECK(m_right_capacity != 0);
			WAWO_ASSERT(m_buffer != NULL);
			WAWO_CONDITION_CHECK(( m_right_capacity + PACK_INCREMENT_RIGHT_SIZE) <= PACK_MAX_RIGHT_CAPACITY);
			m_right_capacity += PACK_INCREMENT_RIGHT_SIZE;
			byte_t* _newbuffer = (byte_t*)::realloc(m_buffer, _capacity());
			WAWO_ALLOC_CHECK(_newbuffer, _capacity());
			m_buffer = _newbuffer;
		}

		void _init_buffer(wawo::u32_t const& left, wawo::u32_t const& right) {
			m_left_capacity = WAWO_MAX2(left, PACK_MIN_LEFT_CAPACITY);
			m_right_capacity = WAWO_MAX2(right, PACK_MIN_RIGHT_CAPACITY);

			reset();

			WAWO_CONDITION_CHECK(m_left_capacity <= PACK_MAX_LEFT_CAPACITY);
			WAWO_CONDITION_CHECK(m_right_capacity <= PACK_MAX_RIGHT_CAPACITY);

			WAWO_CONDITION_CHECK(_capacity() != 0);
			m_buffer = (byte_t*)::malloc(sizeof(byte_t) * _capacity());
			WAWO_ALLOC_CHECK(m_buffer, sizeof(byte_t) * _capacity());

#ifdef ENABLE_DEBUG_MEMORY_ALLOC
			::memset(m_buffer, 'p', _capacity());
#endif
		}

	public:
		explicit packet(wawo::u32_t const& right_capacity = PACK_INIT_RIGHT_CAPACITY , wawo::u32_t const& left_capacity = PACK_INIT_LEFT_CAPACITY ) :
			m_buffer( NULL ),
			m_left_capacity(left_capacity),
			m_read_idx(0),
			m_right_capacity(right_capacity),
			m_write_idx(0)
		{
			_init_buffer(m_left_capacity, m_right_capacity);
		}

		explicit packet(byte_t const* buffer, wawo::u32_t const& len):
			m_buffer(NULL),
			m_left_capacity(PACK_INIT_LEFT_CAPACITY),
			m_read_idx(0),
			m_right_capacity(len),
			m_write_idx(0)
		{
			_init_buffer(m_left_capacity, m_right_capacity);
			write(buffer, len);
		}

		~packet() {
			::free( m_buffer );
		}

		inline wawo::u32_t left_left_capacity() { return (m_buffer == NULL) ? 0 : m_read_idx; }
		inline wawo::u32_t left_right_capacity() { return (m_buffer == NULL) ? 0 : _capacity() - m_write_idx; }

		inline byte_t* begin() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_read_idx;
		}

		inline byte_t* end() const {
			WAWO_ASSERT( m_buffer != NULL );
			return m_buffer + m_write_idx;
		}

		__WW_FORCE_INLINE wawo::u32_t len() const {
			WAWO_ASSERT( m_write_idx >= m_read_idx );
			return m_write_idx - m_read_idx ;
		}

		inline void forward_write_index(wawo::u32_t const& bytes ) {
			WAWO_ASSERT( m_buffer != NULL );
			WAWO_CONDITION_CHECK( (_capacity()-m_write_idx) >= bytes );
			m_write_idx += bytes;
		}

		inline void reset() {
			WAWO_ASSERT( (m_left_capacity>0) && (m_left_capacity < PACK_MAX_LEFT_CAPACITY) );
			m_read_idx = m_write_idx = m_left_capacity ;
		}

		packet* write_left( byte_t const* buffer, wawo::u32_t const& len ) {
			while (len > (left_left_capacity()) ) {
				_extend_leftbuffer_capacity__();
			}

			m_read_idx -= len;
			WAWO_ASSERT(m_read_idx >= 0);
			::memcpy( m_buffer + m_read_idx, buffer, len) ;
			return this ;
		}

		//would result in memmove if left space is not enough
		template <class T, class endian=wawo::bytes_helper::big_endian>
		packet* write_left(T const& t) {
			wawo::u32_t to_write_length = sizeof(T) ;
			while ( to_write_length > (left_left_capacity()) ) {
				_extend_leftbuffer_capacity__();
			}

			m_read_idx -= sizeof(T);
			wawo::u32_t wnbytes = endian::write_impl(t, (m_buffer + m_read_idx) );
			WAWO_ASSERT( wnbytes == sizeof(T) );

			(void)wnbytes;
			return this;
		}

		template <class T, class endian=wawo::bytes_helper::big_endian>
		inline packet* write(T const& t) {
			wawo::u32_t to_write_length = sizeof(T) ;
			while ( to_write_length > (left_right_capacity()) ) {
				_extend_rightbuffer_capacity__();
			}

			m_write_idx += endian::write_impl(t, (m_buffer + m_write_idx) );
			return this;
		}

		inline packet* write( byte_t const* const buffer, wawo::u32_t const& len ) {
			while (len > (left_right_capacity()) ) {
				_extend_rightbuffer_capacity__();
			}
			::memcpy( m_buffer + m_write_idx, buffer, len) ;
			m_write_idx += len;

			return this ;
		}

		template <class T, class endian=wawo::bytes_helper::big_endian>
		inline T read() {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= len() );

			T t = endian::read_impl( m_buffer + m_read_idx, wawo::bytes_helper::type<T>() );
			m_read_idx += sizeof(T);
			return t;
		}

		inline wawo::u32_t read( byte_t* const target, wawo::u32_t const& len_ ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			if ((target == NULL) || len_ == 0) { return 0; }
			const wawo::u32_t c = (len() > len_ ? len_ : len());
			::memcpy(target, m_buffer+m_read_idx, c );
			m_read_idx += c;
			return c;
		}

		inline void skip(wawo::u32_t const& len_ ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT(len_ <= len() );
			m_read_idx += len_;
		}

		template <class T>
		inline T peek() const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( sizeof(T) <= len() );

			T t = wawo::bytes_helper::read_impl( m_buffer + m_read_idx, wawo::bytes_helper::type<T>() );
			return t;
		}

		wawo::u32_t peek( byte_t* const buffer, wawo::u32_t const& len_ ) const {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT(buffer != NULL);
			if ((buffer == NULL) || len_ == 0) { return 0; }

			wawo::u32_t can_peek_size = len_ >= len() ? len() : len_;
			::memcpy(buffer, m_buffer+m_read_idx, can_peek_size);
			return can_peek_size;
		}
	};
}
#endif