#ifndef _WAWO_RING_BUFFER_HPP_
#define _WAWO_RING_BUFFER_HPP_

#include <wawo/core.hpp>

namespace wawo {

	template <class _ItemT>
	class ringbuffer {

		WAWO_DECLARE_NONCOPYABLE(ringbuffer<_ItemT> ) ;

		typedef _ItemT _MyItemT;

		_MyItemT* m_buffer;

		wawo::u32_t m_space;
		wawo::u32_t m_begin;//write
		wawo::u32_t m_end; //read
	public:
		ringbuffer( wawo::u32_t const& space ) :
			m_space(space+1),
			m_begin(0),
			m_end(0),
			m_buffer(NULL)
		{
			WAWO_ASSERT( space>0 );

			m_buffer = new _MyItemT[m_space];
			WAWO_ALLOC_CHECK( m_buffer, (sizeof(_MyItemT)* m_space) );
		}

		~ringbuffer() {
			delete[] m_buffer;
			m_buffer = NULL;
		}

		inline void reset() {
			for( int i=0;i<m_space;++i ) {
				if( m_buffer[i] != NULL ) {
					WAWO_DELETE(m_buffer[i]);
					m_buffer[i] = NULL;
				}
			}
			m_begin = m_end = 0;
		}

		inline wawo::u32_t capacity() const {
			return m_space - 1;
		}

		inline wawo::u32_t left_capacity() const {
			return capacity() - Count();
		}
		inline wawo::u32_t Count() const {
			return ((m_end-m_begin) + m_space ) % m_space;
		}
		//can not read
		inline bool is_empty() const {
			return m_begin == m_end;
		}

		//can not write
		inline bool is_full() const {
			return (m_end+1)%m_space == m_begin;
		}

		bool push( _MyItemT const& item ) {
			if( is_full() ) {
				return false;
			}

			*(m_buffer[m_begin]) = item;
			m_begin = (m_begin+1)%m_space;
			return true;
		}

		bool pop( _MyItemT& item ) {
			if( is_empty() ) {
				return false;
			}

			item = *(m_buffer[m_end]);
			m_end = (m_end+1)%m_space;
			return true;
		}
	};
}

#endif
