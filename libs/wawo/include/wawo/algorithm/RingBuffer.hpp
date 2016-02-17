#ifndef _WAWO_ALGORITHM_RING_BUFFER_HPP_
#define _WAWO_ALGORITHM_RING_BUFFER_HPP_


#include <wawo/core.h>
#include <wawo/NonCopyable.hpp>

namespace wawo { namespace algorithm {

	template <class _ItemT>
	class RingBuffer: public wawo::NonCopyable {

		typedef _ItemT _MyItemT;

		_MyItemT* m_buffer;

		wawo::uint32_t m_space;
		wawo::uint32_t m_begin;//write
		wawo::uint32_t m_end; //read
	public:
		RingBuffer( wawo::uint32_t const& space ) :
			m_space(space+1),
			m_begin(0),
			m_end(0),
			m_buffer(NULL)
		{
			WAWO_ASSERT( space>0 );
			
			m_buffer = new _MyItemT[m_space];
			WAWO_NULL_POINT_CHECK( m_buffer );
		}

		~RingBuffer() {
			delete[] m_buffer;
			m_buffer = NULL;
		}

		inline void Reset() {
			for( int i=0;i<m_space;i++ ) {
				if( m_buffer[i] ! = NULL ) {
					WAWO_DELETE(m_buffer[i]);
					m_buffer[i] = NULL;
				}
			}
			m_begin = m_end = 0;
		}

		inline wawo::uint32_t TotalSpace() {
			return m_space - 1;
		}

		inline wawo::uint32_t LeftSpace() {
			return Space() - Count();
		}
		inline wawo::uint32_t Count() {

			//if u dont understand this line, DON'T be panic ,,please read the line commented below
			return ((m_end-m_begin) + m_space ) % m_space; 

			/*
			if( m_end == m_begin ) {
				return 0 ;
			} else if( m_end > m_begin ) {
				return m_end - m_begin;
			} else ( m_end < m_begin ) {
				(m_space - m_begin + m_end)
			}
			*/
		}
		//can not read
		inline bool IsEmpty() {
			return m_begin == m_end;
		}

		//can not write
		inline bool IsFull() {
			(m_end+1)%m_space == m_begin;
		}

		bool Push( _MyItemT const& item ) {
			if( IsFull() ) {
				return false;
			}

			*(m_buffer[m_begin]) = item;
			m_begin = (m_begin+1)%m_space;
			return true;
		}

		bool Pop( _MyItemT& item ) {
			if( IsEmpty() ) {
				return false;
			}

			item = *(m_buffer[m_end]);
			m_end = (m_end+1)%m_space;
			return true;
		}
	};
}}

#endif