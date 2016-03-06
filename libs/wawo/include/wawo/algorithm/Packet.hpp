#ifndef _WAWO_ALGORITHM_PACKET_HPP_
#define _WAWO_ALGORITHM_PACKET_HPP_

//#include <string>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/bytes_helper.hpp>

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
		enum PacketSettings {
			PS_MIN_LEFT_CAPACITY = (64), //( left part )
			PS_MAX_LEFT_CAPACITY = (1024*1024*8),//maxmium 8M
			PS_MIN_RIGHT_CAPACITY = 64, //net header + left + right
			PS_MAX_RIGHT_CAPACITY = (1024*1024*64), //maxmium 64M
			PS_MIN_CAPACITY = (PS_MIN_LEFT_CAPACITY + PS_MIN_RIGHT_CAPACITY),
			PS_MAX_CAPACITY = (PS_MAX_LEFT_CAPACITY + PS_MAX_RIGHT_CAPACITY),
			PS_INIT_RIGHT_CAPACITY = (1024*8),
			PS_INIT_LEFT_CAPACITY = (PS_MIN_LEFT_CAPACITY),
			PS_INCREMENT_RIGHT_SIZE = (1024*8),
			PS_INCREMENT_LEFT_SIZE = (1024)
		};

	public:
		Packet( uint32_t const& right_capacity = PS_INIT_RIGHT_CAPACITY , uint32_t const& left_capacity = PS_INIT_LEFT_CAPACITY ) :
		m_buffer( NULL ),
		m_left_capacity(0),
		m_left_index(0),
		m_right_capacity(0),
		m_right_index(0)
		{
			_InitBuffer( left_capacity, right_capacity );
		}
		~Packet() {
			WAWO_DELETE( m_buffer );
		}

		Packet( Packet const& copy ) {

			WAWO_ASSERT( m_buffer == NULL );
			m_left_capacity = copy.m_left_capacity ;
			m_right_capacity = copy.m_right_capacity;

			m_buffer = (byte_t*) malloc( sizeof(byte_t) * Capacity() );
			WAWO_NULL_POINT_CHECK(m_buffer);

	#ifdef _DEBUG
			memset(m_buffer, 'w', Capacity() );
	#endif
			memcpy( m_buffer, copy.m_buffer, Capacity() );

			m_left_index = copy.m_left_index;
			m_right_index = copy.m_right_index ;
		}

		Packet& operator= ( Packet const& other) {

			WAWO_ASSERT( this != &other );
			bool should_alloc ;
			if( m_buffer == NULL ) {
				should_alloc = true;
			} else {
			
				WAWO_ASSERT( Capacity() > 0 );
				if( Capacity() < other.Capacity() ) {
					WAWO_DELETE( m_buffer );
					should_alloc = true;
				}
			}

			if(should_alloc) {
				m_buffer = (byte_t*) ::malloc( sizeof(byte_t) * other.Capacity() );
				WAWO_NULL_POINT_CHECK(m_buffer);
			}

			m_left_capacity = other.m_left_capacity ;
			m_right_capacity = other.m_right_capacity;

	#ifdef _DEBUG
			memset(m_buffer, 'w', Capacity() );
	#endif
			memcpy( m_buffer, other.m_buffer, Capacity() );

			m_left_index = other.m_left_index;
			m_right_index = other.m_right_index ;

			return *this;
		}

		inline uint32_t Capacity() const {
			return m_left_capacity + m_right_capacity;
		}

		inline uint32_t LeftCapacity() { return ( (Capacity() == 0) ? 0 : (m_left_index) ); }
		inline uint32_t RightCapacity() { return ( (Capacity() == 0) ? 0 : Capacity() - m_right_index ); }

		inline byte_t* const Begin() const {
			return m_buffer + m_left_index;
		}

		inline byte_t* const End() const {
			return m_buffer + m_right_index;
		}

		inline wawo::uint32_t Length() const {
			WAWO_ASSERT( m_right_index >= m_left_index );
			return m_right_index - m_left_index ;
		}

		inline void MoveForwardRightIndex( uint32_t const& right ) {
			WAWO_CONDITION_CHECK( (Capacity()-m_right_index) >= right );
			m_right_index += right;
		}

		inline void Reset() {
			WAWO_ASSERT( m_left_capacity > 0 );
			WAWO_ASSERT( m_left_capacity < PS_MAX_LEFT_CAPACITY );

			m_left_index = m_right_index = m_left_capacity ;
		}

		Packet* const WriteLeft( byte_t const* buffer, uint32_t const& len ) {

			while ( len > (LeftCapacity()) ) {
				_ExtendLeftBufferCapacity__();
			}

			m_left_index -= len ;
			memcpy( m_buffer + m_left_index, buffer, len ) ;
			return this ;
		}

		//would result in memmove if left space is not enough
		template <class T>
		Packet* const WriteLeft(T const& t) {
			uint32_t to_write_len = sizeof(T) ;
			while ( to_write_len > (LeftCapacity()) ) {
				_ExtendLeftBufferCapacity__();
			}

			m_left_index -= sizeof(T);
			uint32_t write_size = wawo::algorithm::bytes_helper::write_impl(t, (m_buffer + m_left_index) );
			WAWO_ASSERT( write_size == sizeof(T) );

			return this;
		}

		template <class T>
		Packet* const Write(T const& t) {
			uint32_t to_write_len = sizeof(T) ;
			while ( to_write_len > (RightCapacity()) ) {
				_ExtendRightBufferCapacity__();
			}

			m_right_index += wawo::algorithm::bytes_helper::write_impl(t, (m_buffer + m_right_index) );
			return this;
		}

		Packet* const Write( byte_t const* const buffer, uint32_t const& len ) {
			//return Appended bytes

			while ( len > (RightCapacity()) ) {
				_ExtendRightBufferCapacity__();
			}

			memcpy( m_buffer + m_right_index, buffer, len ) ;
			m_right_index += len ;

			return this ;
		}
		
		template <class T>
		T Read() {
			T t = wawo::algorithm::bytes_helper::read_impl( m_buffer + m_left_index, wawo::algorithm::bytes_helper::type<T>() );
			m_left_index += sizeof(T);

			return t;
		}

		uint32_t Read( byte_t* const target, uint32_t const& len ) {
			WAWO_ASSERT( (m_buffer != NULL) );
			WAWO_ASSERT( len <= Length() );

			//uint32_t read = wawo::algorithm::bytes_helper::read_bytes(target, len , m_buffer + m_left_index );

			memcpy(target, m_buffer+m_left_index, len);
			m_left_index += len;
			return len;
		}

		//for performance concern, don't call this function too frequencely,,,
		/*
		template <>
		std::string Read<std::string>() {
			std::string _string_tmp;

			uint32_t _len = Read<uint32_t>();
			WAWO_ASSERT( _len > 0 );
			byte_t* _bytes = (byte_t*) malloc( sizeof(byte_t)*_len );

			uint32_t read_len = Read( _bytes, _len );
			WAWO_ASSERT( read_len == _len );

			std::string _str( (char const*)_bytes, _len);
			free( _bytes );

			return _str;
		}
		*/
	private:
		byte_t* m_buffer ;

		uint32_t	m_left_capacity;
		uint32_t	m_left_index; //point to m_buffer position for read

		uint32_t	m_right_capacity; //the total buffer size
		uint32_t	m_right_index; //point to m_buffer position for write


//		WAWO_REF_PTR<PacketBuffer> m_buffer; ////all the data in this buffer, will be tranlate into Big Endian automatically

	protected:
		inline static uint32_t _EstimateForAlloc( uint32_t const& size ) {

			if( size <= (0x01<<7) ) { //128
				return (0x01<<7) ;
			} else if( size <= (0x01<<8) ) { //256 256bytes
				return (0x01<<8);
			} else if( size <= (0x01<<9) ) { //512 512bytes
				return (0x01<<9);
			} else if( size <= (0x01<<10) ) { //1024 1K
				return (0x01<<10);
			} else if( size <= (0x01<<11) ) { //2048 2K
				return (0x01<<11);
			} else if( size <= (0x01<<12) ) { //4096 4K
				return (0x01<<12);
			} else if( size <= (0x01<<13) ) { //8192 8K
				return (0x01<<13);
			} else if( size <= (0x01<<14) ) { //16384 16K
				return (0x01<<14);
			} else if( size <= (0x01<<15) ) { //32768 32K
				return (0x01<<15);
			} else if( size <= (0x01<<16) ) { //65536 64K
				return (0x01<<16);
			} else if( size <= (0x01<<17) ) { //131072 128K
				return (0x01<<17);
			} else if( size <= (0x01<<18) ) { //262144 256K
				return (0x01<<18);
			} else if( size <= (0x01<<19) ) { //524288 512K
				return (0x01<<19);
			} else if( size <= (0x01<<20) ) { //1048576 1M
				return (0x01<<20);
			} else if (size <= (0x01<<21)) { //2M
				return 0x01<<21;
			} else if( size <= (0x01<<22) ) { //4M
				return 0x01<<22;
			} else if( size <= 0x01<<23 ) { //8M
				return 0x01<<23;
			} else if( size <= 0x01<<24 ) { //16M
				return 0x01<<24;
			} else if ( size <= 0x01<<25 ){ //32M
				return 0x01<<25;
			} else if (size <= 0x01<<26){ //64M
				return 0x01<<26;
			} else {
				WAWO_THROW_EXCEPTION("memory range error for packet!!!");
			}
		}
		void _ExtendLeftBufferCapacity__() {

			WAWO_CONDITION_CHECK( Capacity() != 0 );
			WAWO_CONDITION_CHECK ( ((Capacity() + PS_INCREMENT_LEFT_SIZE) <= PS_MAX_LEFT_CAPACITY) );

			m_left_capacity += PS_INCREMENT_LEFT_SIZE;
			m_buffer = (byte_t*) realloc(m_buffer, Capacity() );
			WAWO_NULL_POINT_CHECK( m_buffer );

			int length = Length();
			uint32_t new_left = m_left_index + PS_INCREMENT_LEFT_SIZE ;

			if( (new_left != m_left_index) && length > 0 ) {
				memmove( m_buffer+new_left, m_buffer + m_left_index, length );
				m_right_index += PS_INCREMENT_LEFT_SIZE;
				m_left_index = new_left;
			}
		}

		void _ExtendRightBufferCapacity__() {
			WAWO_CONDITION_CHECK( Capacity() != 0 );
			WAWO_CONDITION_CHECK ( ((Capacity() + PS_INCREMENT_RIGHT_SIZE) <= PS_MAX_RIGHT_CAPACITY) );

			m_right_capacity += PS_INCREMENT_RIGHT_SIZE;
			m_buffer = (byte_t*) realloc(m_buffer, Capacity() );
			WAWO_NULL_POINT_CHECK( m_buffer );
		}

		void _InitBuffer( uint32_t const& left, uint32_t const& right )  {

			m_left_capacity = WAWO_MAX2(left,PS_MIN_LEFT_CAPACITY);
			m_right_capacity = WAWO_MAX2(right,PS_MIN_RIGHT_CAPACITY);

			WAWO_CONDITION_CHECK( m_left_capacity <= PS_MAX_LEFT_CAPACITY ) ;
			WAWO_CONDITION_CHECK( m_right_capacity <= PS_MAX_RIGHT_CAPACITY ) ;

			//align address to 2^n
			uint32_t total_to_alloc = _EstimateForAlloc( Capacity() );
			WAWO_CONDITION_CHECK( total_to_alloc <= PS_MAX_CAPACITY );

			//recalc right idx
			if( total_to_alloc != Capacity() ) {
				m_right_capacity = total_to_alloc - m_left_capacity;
			}
			Reset();

	//#define WAWO_USE_CALLOC
	#ifdef WAWO_USE_CALLOC
			m_buffer = (byte_t*) calloc( Capacity(), sizeof(byte_t) );
	#else
			m_buffer = (byte_t*) malloc( sizeof(byte_t) * Capacity() ) ;
			WAWO_NULL_POINT_CHECK( m_buffer ) ;
			//set to 00000000000000000
	#ifdef _DEBUG
			memset( m_buffer,'w', Capacity() ) ;
	#endif

	#endif

		}

	};

	/*
	template <>
	Packet* const Packet::Write<std::string>(std::string const& string) {
		WAWO_ASSERT( string.length() > 0 );
		Write( (byte_t const* const)string.c_str(), string.length() );
		return this;
	}
	*/
}}
#endif
