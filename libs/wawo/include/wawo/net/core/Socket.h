#ifndef _WAWO_NET_CORE_SOCKET_H_
#define _WAWO_NET_CORE_SOCKET_H_

#include <wawo/core.h>
#include <wawo/net/core/NetEvent.hpp>

#define WAWO_TRANSLATE_SOCKET_ERROR_CODE(_code) WAWO_NEGATIVE(_code)

namespace wawo { namespace net { namespace core {
	//remark: WSAGetLastError() is jsut a alias for GetLastError(), but there is no gurantee for future change.
	inline int SocketGetLastErrno() {
#ifdef WAWO_PLATFORM_POSIX
		return errno;
#elif defined(WAWO_PLATFORM_WIN)
		return ::WSAGetLastError();
#else
		#error
#endif
	}
}}}

#define WAWO_CHECK_SOCKET_SEND_RETURN_V(v) \
	do { \
		WAWO_ASSERT(v == wawo::OK || \
					v == wawo::E_SOCKET_SEND_BUFFER_NOT_ENOUGH || \
					v == wawo::E_SOCKET_NOT_CONNECTED || \
					v == wawo::E_WSAECONNABORTED || \
					v == wawo::E_WSAECONNRESET || \
					v == wawo::E_WSAENOTSOCK || \
					v == wawo::E_ECONNRESET || \
					v == wawo::E_WSAEHOSTDOWN || \
					v == wawo::E_WSAESHUTDOWN || \
					v == wawo::E_ECONNRESET || \
					v == wawo::E_EPIPE || \
					v == wawo::E_EBADF || \
					v == wawo::E_ESHUTDOWN || \
					v == wawo::E_EHOSTDOWN || \
					v == wawo::E_EACCESS || \
					v == wawo::E_ENOMEM || \
					v == wawo::E_ENOTSOCK || \
					v == wawo::E_ENOBUFS \
			); \
	}while(0);


#ifndef EWOULDBLOCK
	#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#ifndef WSAEWOULDBLOCK
	#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#ifndef EAGAIN //EAGAIN IS NOT ALWAYS THE SAME AS EWOULDBLOCK
	#define EAGAIN EWOULDBLOCK
#endif

#ifndef EISCONN
	#define EISCONN WSAEISCONN
#endif

#ifndef ENOTINITIALISED
	#define ENOTINITIALISED WSANOTINITIALISED
#endif

#define IS_ERRNO_EQUAL_WOULDBLOCK(_errno) ((_errno==EWOULDBLOCK)||(_errno==EAGAIN)||(_errno==WSAEWOULDBLOCK))
#define IS_ERRNO_EQUAL_CONNECTING(_errno) ((_errno==WSAEWOULDBLOCK)||(_errno==EINPROGRESS))

#define IS_ERRNO_NOT_SOCKET_ERROR(_errno) (_errno ==0 || _errno == wawo::E_SOCKET_REMOTE_GRACE_CLOSE )

//#define TEST_FLAG(test_flag, flags) ( (test_flag & flags ) )

//IN milliseconds
#define WAWO_MAX_ASYNC_WRITE_PERIOD			(15*1000)
#define WAWO_MAX_ASYNC_RECV_PERIOD			(15*1000)

#define __SEND_MAX_TRY_TIME__ (3)
//for bad network condition ,please increae this value, in milliseconds
#define __SEND_TRY_TIME_ADDITION_TIME__ (1)

#ifndef _DEBUG
	#undef WAWO_CHECK_SOCKET_SEND_RETURN_V
	#define WAWO_CHECK_SOCKET_SEND_RETURN_V(V)
#endif

#define WAWO_MAX_ACCEPTS_ONE_TIME 128


#include <wawo/SmartPtr.hpp>
#include <wawo/SafeSingleton.hpp>
#include <wawo/time/time.hpp>
#include <wawo/thread/Mutex.h>

#include <wawo/algorithm/BytesRingBuffer.hpp>
#include <wawo/net/core/SocketAddr.h>

#include <wawo/net/core/Dispatcher_Abstract.hpp>
#include <wawo/algorithm/Packet.hpp>

namespace wawo { namespace net { namespace core {
#ifdef WAWO_PLATFORM_POSIX
	typedef socklen_t socklen_t;
#elif defined(WAWO_PLATFORM_WIN)
	typedef int socklen_t ;
#else
	#error
#endif
}}}

namespace wawo { namespace net { namespace core {
	using namespace wawo::thread;

	enum SockBufferConfigType {
		SBCT_EXTREM_LARGE,
		SBCT_SUPER_LARGE,
		SBCT_LARGE,
		SBCT_MEDIUM_UPPER,
		SBCT_MEDIUM,
		SBCT_MEDIUM_LOWER,
		SBCT_TINY,
		SBCT_SUPER_TINY,
		SBCT_EXTREM_TINY,
		SBCT_MAX
	};

	struct SockBufferConfig {
		uint32_t snd_size;
		uint32_t rcv_size;
	};

	static SockBufferConfig BufferConfigs[SBCT_MAX] =
	{
		{1024*1024*4,	1024*1024*4}, //4M //extrem large
		{1024*1024,		1024*1024}, //1M //super large
		{1024*512,		1024*512}, //512K large
		{1024*128,		1024*128}, //128k medium upper
		{1024*64,		1024*64}, //64k medium
		{1024*32,		1024*32}, //32k medium_lower
		{1024*16,		1024*16}, //16k tiny
		{1024*8,		1024*8}, //8k super tiny
		{1024*4,		1024*4}, //4k , extrem_tiny
	};

	enum SockBufferRange {
		SOCK_SND_MAX_SIZE = 1024*1024*4,
		SOCK_SND_MIN_SIZE = 1024*4,
		SOCK_RCV_MAX_SIZE = 1024*1024*4,
		SOCK_RCV_MIN_SIZE = 1024*4
	};

	class Socket :
		virtual public wawo::RefObject_Abstract
	{

	public:
		enum SocketFds {
			FD_ERROR		= -1,
			FD_INIT			= -2,
			FD_CLOSED		= -3
		};

		enum Family {
			FAMILY_NONE,
			FAMILY_PF_INET,
			FAMILY_AF_INET,
			FAMILY_AF_UNIX,
			FAMILY_PF_UNIX,
		};

		enum SocketMode {
			SM_NONE,
			SM_ACTIVE,
			SM_PASSIVE,
			SM_LISTENER
		};

		enum Type {
			TYPE_NONE,
			TYPE_STREAM,
			TYPE_DGRAM
		};

		enum Protocol {
			PROTOCOL_NONE,
			PROTOCOL_UDP,
			PROTOCOL_TCP,
		};

		enum Option {
			OPTION_NONE			= 0,
			OPTION_BROADCAST	= 0x001, //only for UDP
			OPTION_REUSEADDR	= 0x002,
			OPTION_NON_BLOCKING	= 0x004,
			OPTION_NODELAY		= 0x008, //only for TCP
		};

		enum SocketCloseFlag {
			SSHUT_NONE = 0,
			SSHUT_RD = 0x01,
			SSHUT_WR = 0x02,
			SSHUT_RDWR = (SSHUT_RD|SSHUT_WR)
		};

		enum SocketState {
			STATE_IDLE = 0,
			STATE_OPENED,
			STATE_BINDED,
			STATE_LISTEN,
			STATE_CONNECTING,// for async connect
			STATE_CONNECTED,
			STATE_RD_CLOSED,
			STATE_WR_CLOSED,
			STATE_RDWR_CLOSED,
		};

		enum LockType {
			L_SOCKET = 0,
			L_READ,
			L_WRITE,
			L_MAX
		};

		// Socket( SOCKET fd ); //for unix domain , as we can not get socket addr from anubis
		explicit Socket( int const& fd, SocketAddr const& addr, Socket::SocketMode const& sm , SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto , Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ); //by pass a connected socket fd
		explicit Socket( SocketAddr const& addr, Socket::Type const& type, Socket::Protocol const& proto, Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ); //init a empty socket object
		explicit Socket( SocketAddr const& addr,SockBufferConfig const& sbc, Socket::Type const& type, Socket::Protocol const& proto, Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ); //init a empty socket object
		explicit Socket( SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ); //init a empty socket object

		virtual ~Socket();

		inline SocketMode& GetMode() { return m_sm; }
		inline SocketMode const& GetMode() const { return m_sm; }

		inline bool IsPassive() const { return m_sm == SM_PASSIVE; }
		inline bool IsActive() const {return m_sm == SM_ACTIVE;}
		inline bool IsListener() const {return m_sm ==SM_LISTENER;}

		inline bool IsDataSocket() { return GetMode() != SM_LISTENER ;}
		inline bool IsListenSocket() { return GetMode() == SM_LISTENER; }

		inline int& GetFd() { return m_fd; }
		inline int const& GetFd() const { return m_fd ;}

		SocketAddr const& GetAddr();
		SocketAddr const& GetAddr() const;

		int Open();
		int Close( int const& ec = wawo::OK);
		int Shutdown(int const& shut_flag, int const& ec = wawo::OK);

		inline bool IsIdle() {return m_state == Socket::STATE_IDLE ;}
		inline bool IsOpened() {return m_state == Socket::STATE_OPENED ;}
		inline bool IsBinded() {return m_state == Socket::STATE_BINDED; }
		inline bool IsConnected() {return m_state == Socket::STATE_CONNECTED; }
		inline bool IsClosed() { return (m_fd<0) || (m_state == Socket::STATE_RDWR_CLOSED); }
		inline bool IsReadClosed() {return (m_fd<0) || (m_shutdown_flag&SSHUT_RD) ;}
		inline bool IsWriteClosed() {return (m_fd<0) || (m_shutdown_flag&SSHUT_WR) ;}

		int Bind( SocketAddr const& addr );
		int Listen( int const& backlog );
		inline bool IsListening() { return (m_fd>0) && (m_state == STATE_LISTEN); }

		//return socketFd if success, otherwise return -1 if an error is set
		uint32_t Accept( WAWO_REF_PTR<Socket> sockets[], uint32_t const& size, int& ec_o );
		int Connect();
		//int Connect( SocketAddr& addr );

		uint32_t Send( byte_t const* const buffer, uint32_t const& size, int& ec_o, int const& flags = 0 );
		uint32_t Recv(byte_t* const buffer_o, uint32_t const& size, int& ec_o, int const& flags = 0 );


		uint32_t SendTo( const byte_t* buff, wawo::uint32_t const& size, const SocketAddr& addr, int& ec_o );
		uint32_t RecvFrom( byte_t* const buff_o, wawo::uint32_t const& size, SocketAddr& addr, int& ec_o );

		int const& GetErrorCode() const ;

		inline bool IsNoDelay() { return ((m_option & Socket::OPTION_NODELAY) != 0) ; }

		int TurnOffNoDelay();
		int TurnOnNoDelay();

		int TurnOnNonBlocking();
		int TurnOffNonBlocking();
		inline bool IsNonBlocking() {return ( (m_option & Socket::OPTION_NON_BLOCKING) != 0) ;}

		//must be called between open and connect|listen
		int SetSndBufferSize( int const& size );
		int GetSndBufferSize( int& size );

		//must be called between open and connect|listen
		int SetRcvBufferSize( int const& size );
		int GetRcvBufferSize( int& size );

		int GetLinger( bool& on_off, int& linger_t );
		int SetLinger( bool const& on_off, int const& linger_t = 30 /* in seconds */ ) ;

		inline SocketState const& GetState() { return m_state;}
		inline SocketState const& GetState() const { return m_state;}

		inline SpinMutex& GetLock( LockType const& lt) {  WAWO_ASSERT( lt < L_MAX );return m_mutexes[lt]; }
		inline SpinMutex const& GetLock( LockType const& lt) const {  WAWO_ASSERT( lt < L_MAX ); return m_mutexes[lt]; }

		inline bool IsConnecting() { return m_is_connecting; }

	protected:
		int _Shutdown(int const& shut_flag, int const& ec );
		int _Connect();

		int SetOptions( int const& options );

		SocketMode m_sm; //
		Family m_family;
		Type m_type;
		Protocol m_proto;
		int m_option;

		SocketAddr m_addr ;

		int m_fd;
		int m_ec;

		SocketState		m_state;
		uint8_t			m_shutdown_flag;

		SpinMutex m_mutexes[L_MAX];

		SockBufferConfig m_sbc; //socket buffer setting

		bool m_is_connecting:1; //lock by w-mutex
	};

	template <class _PacketProtocolT>
	class SocketEvent;

	enum SocketIOState {
		//read scope
		S_READ_IDLE,
		S_READ_POSTED,
		S_READING,
		S_READ_CONTINUE, //edge io mode

		//write scope
		S_WRITE_IDLE,
		S_WRITE_POSTED,
		S_WRITING,

		//socket scope, for connect error check
		S_CONNECT_IDLE,
		S_CONNECT_CHECK_POSTED,
		S_CONNECT_CHECKING
	};

	template <class _PacketProtocolT>
	class BufferSocket:
		public Socket,
		public Dispatcher_Abstract< SocketEvent<_PacketProtocolT> >
	{

	public:
		typedef _PacketProtocolT MyProtocolT;
		typedef BufferSocket<_PacketProtocolT> MyT;
		typedef SocketEvent<_PacketProtocolT> SocketEventT;
		typedef Dispatcher_Abstract< SocketEvent<_PacketProtocolT> > _MyDispatcherT;

		explicit BufferSocket(int const& fd, SocketAddr const& addr, SocketMode const& sm, SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ):
			Socket(fd,addr,sm,sbc,type,proto,family,option),
			m_rs(S_READ_IDLE),
			m_ws(S_WRITE_IDLE ),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_protocol(NULL)
		{
			_Init();
		}

		explicit BufferSocket( SocketAddr const& addr, Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ):
			Socket(addr,type,proto,family,option),
			m_rs(S_READ_IDLE),
			m_ws(S_WRITE_IDLE ),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_protocol(NULL)
		{
			_Init();
		}

		explicit BufferSocket( SocketAddr const& addr, SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ):
			Socket(addr,sbc,type,proto,family,option),
			m_rs(S_READ_IDLE),
			m_ws(S_WRITE_IDLE ),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_protocol(NULL)
		{
			_Init();
		}

		explicit BufferSocket(SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family = Socket::FAMILY_AF_INET, Socket::Option const& option = Socket::OPTION_NONE ):
			Socket(sbc,type,proto,family, option),
			m_rs(S_READ_IDLE),
			m_ws( S_WRITE_IDLE ),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_protocol(NULL)
		{
			_Init();
		}

		~BufferSocket() {

			if( IsNonBlocking() &&
				IsDataSocket() &&
				!IsWriteClosed() )
			{
				//last time to flush
				Flush();
			}

			_Deinit();
		}

		void _Init() {
			WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );

			m_tsb = (byte_t*) malloc ( sizeof(byte_t)*m_sbc.snd_size ) ;
			WAWO_CONDITION_CHECK( m_tsb != NULL );

#ifdef _DEBUG
			memset( m_tsb, 'd', m_sbc.snd_size );
#endif
			WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
			m_trb = (byte_t*) malloc( sizeof(byte_t)*m_sbc.rcv_size ) ;
			WAWO_CONDITION_CHECK( m_trb != NULL );

#ifdef _DEBUG
			memset( m_trb, 'd', m_sbc.rcv_size );
#endif
			WAWO_ASSERT( m_rb == NULL );
			m_rb = new wawo::algorithm::BytesRingBuffer( m_sbc.rcv_size*4 ) ;
			WAWO_NULL_POINT_CHECK( m_rb );

			WAWO_ASSERT( m_sb == NULL );
			m_sb = new wawo::algorithm::BytesRingBuffer( m_sbc.snd_size*4 ) ;
			WAWO_NULL_POINT_CHECK( m_sb );

			WAWO_ASSERT ( m_protocol == NULL );
			m_protocol = new MyProtocolT();
			WAWO_NULL_POINT_CHECK(m_protocol);
		}

		void _Deinit() {
			free( m_tsb );
			m_tsb = NULL;

			free( m_trb );
			m_trb = NULL;

			WAWO_ASSERT( m_rb != NULL );
			WAWO_DELETE( m_rb );

			WAWO_ASSERT( m_sb != NULL );
			WAWO_DELETE( m_sb );

			WAWO_ASSERT( m_protocol != NULL );
			WAWO_DELETE( m_protocol );
		}

		uint32_t Accept( WAWO_REF_PTR<MyT> sockets[], uint32_t const& size, int& ec_o ) {

			WAWO_ASSERT( m_sm == SM_LISTENER );
			uint32_t count = 0;

			if( m_state != STATE_LISTEN ) {
				ec_o = wawo::E_SOCKET_INVALID_STATE;
				return count ;
			}

			ec_o = wawo::OK;

			sockaddr_in addr_in;
			socklen_t addr_len = sizeof(addr_in);

			do {
				SocketAddr addr;

				int fd = accept( m_fd, reinterpret_cast<sockaddr*>(&addr_in), &addr_len );
				if( -1 == fd ) {
					int ec = SocketGetLastErrno();
					if( !IS_ERRNO_EQUAL_WOULDBLOCK(ec) ) {
						ec_o = WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec);
					}
					break;
				}

				addr.SetHostSequencePort( ntohs(addr_in.sin_port) );
				addr.SetHostSequenceUlongIp( ntohl(addr_in.sin_addr.s_addr) );

				WAWO_REF_PTR<MyT> socket( new MyT(fd, addr, SM_PASSIVE, m_sbc, m_type, m_proto, m_family, OPTION_NONE) );

				sockets[count++] = socket;
			} while( count<size );

			if( count == size ) {
				ec_o=wawo::E_SOCKET_ACCEPT_MAY_MORE;
			}

			return count ;
		}

		inline SocketIOState const& GetRState() {
			return m_rs;
		}

		inline SocketIOState const& GetWState() {
			return m_ws;
		}

		inline void SetRState( SocketIOState const& state ) {
			m_rs = state;
		}
		inline void SetWState( SocketIOState const& state ) {
			m_ws = state;
		}

		int SendPacket( WAWO_SHARED_PTR<Packet> const& packet ) {
			WAWO_ASSERT( m_protocol != NULL );
			WAWO_SHARED_PTR<Packet> packet_out ;
			int rt = m_protocol->Encode ( packet, packet_out );

			if( rt != wawo::OK ) {
				return rt;
			}

			int ec;
			return (Send( packet_out->Begin(), packet_out->Length(), ec ) == packet_out->Length()) ? wawo::OK : ec ;
		}

		// return count, set error if any, blocking read
		uint32_t ReceivePackets( WAWO_SHARED_PTR<Packet> packets[], uint32_t const& size, int& ec_o ) {
			LockGuard<SpinMutex> lg( m_mutexes[L_READ] );

			uint32_t pumped = __Pump(ec_o);
			if( pumped > 0 ) {
				return _DecodePackets( packets, size , ec_o );
			}

			return 0;
		}

		uint32_t Send( byte_t const* const buffer, uint32_t const& size, int& ec_o, int const& flags = 0 ) {

			WAWO_ASSERT( buffer != NULL );
			WAWO_ASSERT( size > 0 );
			WAWO_ASSERT( size <= m_rb->TotalSpace() );

				if ( Socket::IsClosed() ) {
				ec_o = wawo::E_SOCKET_NOT_CONNECTED ;
				return 0;
			}

			LockGuard<SpinMutex> lg( m_mutexes[L_WRITE]);
			if( IsNonBlocking() ) {
				WAWO_ASSERT( m_sb != NULL );

				uint32_t wcount = 0;
				if( m_sb->BytesCount() > 0 ) {

					if( m_sb->LeftSpace() > size ) {
						wcount = m_sb->Write( (byte_t*)(buffer), size );
						WAWO_ASSERT( wcount == size );
						WAWO_LOG_DEBUG("buffer_socket","[#%d:%s] BufferSocket::Send(): ringbuffer not empty, write to send ringbuffer, write_count: %d" , m_fd, m_addr.AddressInfo(), wcount ) ;
					} else {
						ec_o = wawo::E_SOCKET_SEND_BUFFER_NOT_ENOUGH ;
						WAWO_LOG_WARN( "buffer_socket", "[#%d:%s]m_sb AvailableSpace()=%d < %d, write count: 0, pls try,,,", m_fd, m_addr.AddressInfo(),m_sb->LeftSpace(), size );
					}

					return wcount;
				}
			}

			uint32_t sent = Socket::Send(buffer,size,ec_o);

			if( sent<size ) {

				if( ec_o == wawo::E_SOCKET_SEND_IO_BLOCK ) {
					WAWO_ASSERT( IsNonBlocking() );
					WAWO_ASSERT( m_sb != NULL );

					//write to send to ring buffer, and register this socket to CHECK_WRITE observer
					//calc the rest part point, and push it into send buffer
					uint32_t to_buffer_c = size-sent ;

					if( m_sb->LeftSpace() < to_buffer_c ) {
						WAWO_LOG_WARN("buffer_socket","[#%d:%s] Socket::Send() blocked, socket::sent: alread sent %d," , m_fd, m_addr.AddressInfo(), sent ) ;
						ec_o = wawo::E_SOCKET_SEND_BUFFER_NOT_ENOUGH ;
					} else {
						wawo::uint32_t write_c = m_sb->Write( (byte_t*)(buffer) + sent, to_buffer_c ) ;
						WAWO_ASSERT( to_buffer_c == write_c );
						WAWO_LOG_WARN("buffer_socket","[#%d:%s] Socket::Send() blocked, socket::sent: %d, write to send ringbuffer: %d" , m_fd, m_addr.AddressInfo(), sent, write_c ) ;
						sent += write_c;

						WAWO_REF_PTR<SocketEventT> evt( new SocketEventT(WAWO_REF_PTR<MyT>(this), SE_SEND_BLOCK ));
						_MyDispatcherT::Raise( evt );
					}
				}
			}

			return sent;
		}

		uint32_t Recv( byte_t* const buffer_o, uint32_t const& size, int& ec_o, int const& flag = 0 ) {
			LockGuard<SpinMutex> lg( m_mutexes[L_READ]);
			return Socket::Recv( buffer_o, size, ec_o, flag );
		}

		inline uint32_t _DecodePackets( WAWO_SHARED_PTR<Packet> packets[], uint32_t const& size, int& ec_o ) {
			WAWO_ASSERT( m_rb->BytesCount() > 0 );
			WAWO_ASSERT( m_protocol != NULL );

			return m_protocol->DecodePackets( m_rb, packets, size, ec_o );
		}

		uint32_t __Pump(int& ec_o) {

			WAWO_ASSERT( IsDataSocket() );
			WAWO_ASSERT( m_rb != NULL );

			bool try_again;
			uint32_t pumped = 0;
			ec_o = wawo::OK;

			do {

				uint32_t left_space = m_rb->LeftSpace() ;
				if( 0 == left_space ) {
					WAWO_LOG_WARN("buffer_socket", "[#%d:%s]m_rb->LeftSpace() == 0", m_fd, m_addr.AddressInfo());
					ec_o = wawo::E_SOCKET_RECV_BUFFER_FULL ;
					break;
				}

				uint32_t can_trb_recv_s = ( m_sbc.rcv_size <= left_space) ?  m_sbc.rcv_size : left_space ;
				uint32_t recv_c = Socket::Recv( m_trb, can_trb_recv_s, ec_o );

				if( recv_c>0 ) {
#ifdef _DEBUG
	//#define _TEST_ONE_BYTES_ISSUE
#endif

#ifdef _TEST_ONE_BYTES_ISSUE
					static uint32_t _one_bytes_count = 0;
					if( recv_c == 1 ) {
						++_one_bytes_count;
					} else {
						_one_bytes_count = 0;
					}
					if( _one_bytes_count>5 ) {
						WAWO_ASSERT(!"__one_bytes_issue____");
					}
#endif

#ifdef WAWO_IO_MODE_EPOLL_USE_ET
					try_again = ( ec_o != wawo::E_SOCKET_RECV_IO_BLOCK );
#else
					try_again = ((recv_c == can_trb_recv_s ) && ec_o == wawo::OK );
#endif
					wawo::uint32_t _rb_wc = m_rb->Write(m_trb, recv_c ) ;
					WAWO_ASSERT( recv_c == _rb_wc );

					pumped += recv_c ;
				} else {
					WAWO_ASSERT( ec_o != wawo::OK );
					try_again = false;
				}
			} while( try_again && IsNonBlocking() ) ;

			WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]__Pump, pump bytes: %d, ec: %d", m_fd, m_addr.AddressInfo() , pumped, ec_o );
			return pumped;
		}

		void Pump(int& ec_o) {
			WAWO_ASSERT( IsDataSocket() );
			WAWO_ASSERT( IsNonBlocking() );
			uint32_t pumped;
			uint32_t pumped_total=0;

			LockGuard<SpinMutex> lg( m_mutexes[L_READ]);
			do {
				pumped = __Pump(ec_o);
				pumped_total += pumped;

				if( pumped>0 ) {

					do {

						WAWO_SHARED_PTR<Packet> packets[MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET];
						uint32_t count = _DecodePackets( packets, MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET,ec_o );
						for(uint32_t i=0;i<count;i++) {
							WAWO_REF_PTR<SocketEventT> evt( new SocketEventT(WAWO_REF_PTR<MyT>(this), SE_PACKET_ARRIVE, packets[i]) );
							_MyDispatcherT::Trigger( evt );
						}

						if( ec_o == wawo::E_SOCKET_MAY_HAVE_MORE_PACKET ) {
							WAWO_LOG_WARN("buffer_socket", "[#%d:%s]may have more than %d packet to parse in socket ringbuffer",m_fd , m_addr.AddressInfo() , MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET );
						}
					} while( ec_o == wawo::E_SOCKET_MAY_HAVE_MORE_PACKET );
				}
			} while( (pumped == 0) && (ec_o == wawo::E_SOCKET_RECV_BUFFER_FULL) );
		}

		inline void HandleAsyncConnected(int& ec_o) {
			LockGuard<SpinMutex> _lg(m_mutexes[L_SOCKET]);
			ec_o = wawo::OK;

			if( m_state == STATE_RDWR_CLOSED ) {
				ec_o = wawo::E_SOCKET_INVALID_STATE;
				return ;
			}

			WAWO_ASSERT( m_fd > 0 );
			WAWO_ASSERT( m_sm == SM_ACTIVE || m_sm == SM_PASSIVE);
			WAWO_ASSERT( IsNonBlocking() );

			if( m_sm == SM_ACTIVE ) {
				WAWO_ASSERT( m_ws == S_WRITE_POSTED );
				WAWO_ASSERT( m_state == STATE_CONNECTING );
				WAWO_ASSERT( m_is_connecting );
				m_ws = S_WRITING;

				int iError = 0;
				socklen_t opt_len = sizeof(int);

				if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (char*)&iError, &opt_len) < 0) {
					WAWO_LOG_FATAL( "buffer_socket", "[#%d:%s]getsockopt failed: %d",m_fd,m_addr.AddressInfo(), iError );
				}

				if( iError == 0 ) {
					WAWO_LOG_DEBUG( "buffer_socket", "[#%d:%s]socket connected", m_fd, m_addr.AddressInfo() );
					m_state = STATE_CONNECTED ;
					m_is_connecting = false;
				}
				m_ws = S_WRITE_IDLE;
			}

#ifdef _DEBUG
			else {
				WAWO_ASSERT( m_sm == SM_PASSIVE );
				WAWO_ASSERT( m_state == STATE_CONNECTED );
			}
#endif

			if( m_state == STATE_CONNECTED ) {


#ifdef _TEST_SOCKET_SHUTDOWN
					int rt ;
					char tmp[10240] = {0};
					//test linger

					rt = send( m_fd, tmp, 10240,0 );
					WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]send rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );


					rt = send( m_fd, tmp, 4096,0 );
					WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]send rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );


					//rt = send( m_fd, tmp, 4096,0 );
					//WAWO_LOG_DEBUG("socket", "[#%d:%s] send rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );

					//rt= recv( m_fd,tmp,128,0 );
					//WAWO_LOG_DEBUG("socket", "[#%d:%s] recv rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );

					//test linger
					rt = WAWO_CLOSE_SOCKET(m_fd);
					//rt = shutdown( m_fd, SHUT_RDWR );
					WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]shutdown rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );

					rt = recv( m_fd,tmp,128,0 );
					WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]recv rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );
#endif
			}
		}

		inline void HandleAsyncRecv(int& ec_o) {

			WAWO_ASSERT( IsNonBlocking() );
			ec_o = wawo::OK;

			WAWO_ASSERT( m_rs == S_READ_POSTED );
			WAWO_LOG_DEBUG("buffer_socket","[#%d:%s]socket async read begin", m_fd, m_addr.AddressInfo() );
			m_rs = S_READING ;

			uint32_t pumped_total = 0;
			uint32_t pumped ;

			do {
				pumped = __Pump(ec_o);
				pumped_total += pumped;

				if( pumped>0 ) {
					do {

						WAWO_SHARED_PTR<Packet> packets[MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET];
						uint32_t count = _DecodePackets( packets, MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET,ec_o );
						for(uint32_t i=0;i<count;i++) {
							WAWO_REF_PTR<SocketEventT> evt( new SocketEventT(WAWO_REF_PTR<MyT>(this), SE_PACKET_ARRIVE, packets[i]) );
							_MyDispatcherT::Trigger( evt );
						}

						if( ec_o == wawo::E_SOCKET_MAY_HAVE_MORE_PACKET ) {
							WAWO_LOG_WARN("buffer_socket", "[#%d:%s]may have more than %d packet to parse in socket ringbuffer",m_fd , m_addr.AddressInfo() , MAX_PACKET_COUNT_TO_PARSE_PER_TIME_FOR_ASYNC_SOCKET );
						}
					} while( ec_o == wawo::E_SOCKET_MAY_HAVE_MORE_PACKET );
				}
			} while( (pumped == 0) && (ec_o == wawo::E_SOCKET_RECV_BUFFER_FULL) );

			m_rs = S_READ_IDLE ;
			WAWO_LOG_DEBUG("buffer_socket","[#%d:%s]socket async read end, Socket::AsyncRecv received bytes: %d, read state: %d", m_fd, m_addr.AddressInfo(),pumped_total, m_rs );
		}

		inline void HandleAsyncSend(uint32_t& left, int& ec_o) {

			WAWO_ASSERT( IsNonBlocking() );
			WAWO_ASSERT( m_sb != NULL );
			ec_o = wawo::OK ;

			WAWO_LOG_DEBUG("buffer_socket","[#%d:%s]socket AsyncSend begin", m_fd, m_addr.AddressInfo() );
			WAWO_ASSERT( m_ws == S_WRITE_POSTED );

			m_ws = S_WRITING ;

			uint32_t flushed = __Flush( left, ec_o );

			if( left == 0 ) {
				WAWO_ASSERT( ec_o == wawo::OK );
				m_async_wt = 0;
			} else {

				if( ec_o == wawo::E_SOCKET_SEND_IO_BLOCK ) {
					uint64_t current = wawo::time::curr_milliseconds() ;

					if( (flushed == 0) && (m_async_wt != 0) && ((current - m_async_wt) > m_delay_wp ) ) {
						ec_o = wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED;
					} else {
						if( (m_async_wt==0) || (flushed>0) ) {
							m_async_wt = current;//timer start
						}
					}
				}
			}

			m_ws = S_WRITE_IDLE ;
			WAWO_LOG_DEBUG( "buffer_socket", "[#%d:%s]Socket::AsyncSend end sent: %d, left: %d", m_fd, m_addr.AddressInfo(), flushed, left );
		}

	//must lock first, flush until left == 0 || get send block
	uint32_t __Flush( uint32_t& left, int& ec_o ) {
		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( IsNonBlocking() );

		uint32_t flushed = 0;
		left = m_sb->BytesCount();
		ec_o = wawo::OK;
		do {

			if( left == 0 ) { break;}
			uint32_t to_sent_s = m_sb->Peek( m_tsb, m_sbc.snd_size );
			uint32_t sent = Socket::Send( m_tsb, to_sent_s, ec_o ) ;
			flushed += sent ;

			if( sent != to_sent_s ) {
				WAWO_LOG_WARN( "buffer_socket", "[#%d:%s]Socket::_Send, try to send: %d, but only sent: %d, ec: %d", m_fd, m_addr.AddressInfo(), to_sent_s, sent, ec_o );
			}

			if( sent > 0 ) {
				m_sb->Skip(sent);
			}
			left = m_sb->BytesCount() ;
		} while ( (left>0) && (ec_o == wawo::OK ) );

		WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s]__Flush, flushed bytes: %d, left: %d, ec: %d", m_fd, m_addr.AddressInfo() , flushed, left, ec_o );
		return flushed;
	}

	void Flush( uint32_t const& time = 1000 /* in milliseconds*/ ) {
		WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s] Flush begin, time: %d", m_fd, m_addr.AddressInfo() , time );

		WAWO_ASSERT( IsNonBlocking() );
		WAWO_ASSERT( IsDataSocket() );

		uint32_t left;
		uint32_t flushed_total = 0;
		int ec_o ;
		LockGuard<SpinMutex> lg( m_mutexes[L_WRITE]);

		uint64_t begin_time = wawo::time::curr_milliseconds();
		uint64_t last_time;

		do {
			flushed_total += __Flush(left,ec_o);
			uint64_t now = wawo::time::curr_milliseconds();
			last_time = now - begin_time;
		} while( (ec_o == wawo::E_SOCKET_SEND_IO_BLOCK) && (left > 0) && (last_time<time) );

		if( ec_o != wawo::OK ) {
			WAWO_LOG_WARN("buffer_socket", "[#%d:%s] Flush, flushed bytes: %d, left: %d, ec: %d", m_fd, m_addr.AddressInfo() , flushed_total, left, ec_o );
		}

		WAWO_LOG_DEBUG("buffer_socket", "[#%d:%s] Flush end, flushed bytes: %d, left: %d, ec: %d", m_fd, m_addr.AddressInfo() , flushed_total, left, ec_o );
	}

	void HandleAsyncAccept( int& ec_o ) {

		WAWO_ASSERT( m_rs == S_READ_POSTED );
		m_rs = S_READING;
		do {

			WAWO_REF_PTR<MyT> sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
			uint32_t count = Accept( sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec_o );

			for( uint32_t i=0;i<count;i++ ) {
				WAWO_REF_PTR<SocketEventT> evt( new SocketEventT(WAWO_REF_PTR<MyT>(this),SE_ACCEPTED, EventData( (void*)(sockets[i].Get())) ) );
				_MyDispatcherT::Trigger( evt );
			}

		} while( ec_o == wawo::E_SOCKET_ACCEPT_MAY_MORE );
		m_rs = S_READ_IDLE;
	}

	//you must Lock( Socket::L_READ_BUFFER ); for mul thread safety
	inline wawo::algorithm::BytesRingBuffer* const& GetRBuffer() {
		WAWO_ASSERT( m_rb != NULL );
		return m_rb ;
	}

	//you must Lock( Socket::L_WRITE_BUFFER ); for mul thread safety
	inline wawo::algorithm::BytesRingBuffer* const& GetWBuffer() {
		WAWO_ASSERT( m_rb != NULL );
		return m_sb;
	}

	private:
		//WAWO_REF_PTR<MySocketObserverT> m_observer;

		SocketIOState m_rs; //m_read state
		SocketIOState m_ws; //m_write state

		wawo::algorithm::BytesRingBuffer* m_sb; // buffer for send
		wawo::algorithm::BytesRingBuffer* m_rb; // buffer for recv

		byte_t* m_tsb; //tmp send buffer
		byte_t* m_trb; //tmp read buffer

		uint32_t m_delay_wp;
		uint64_t m_async_wt;

		MyProtocolT* m_protocol;
	};

}}}
#endif
