#include <wawo/core.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/net/core/Socket.h>

//#define _TEST_SOCKET_SHUTDOWN

namespace wawo { namespace net { namespace core {


	Socket::Socket( int const& fd, SocketAddr const& addr, Socket::SocketMode const& sm , SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto , Socket::Family const& family, Socket::Option const& option  ):
		m_sm(sm),
		m_family(family),
		m_type(type),
		m_proto(proto),
		m_option(option),

		m_addr(addr),
		m_fd(fd),
		m_ec(wawo::OK),
		m_state( STATE_CONNECTED ),
		m_shutdown_flag(SSHUT_NONE),

		m_sbc(sbc),
		m_is_connecting(false)
	{
		WAWO_ASSERT( fd > 0 );
		
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );

		int buff_init_rt = SetSndBufferSize( m_sbc.snd_size );
		WAWO_ASSERT( buff_init_rt == wawo::OK );

		buff_init_rt = SetRcvBufferSize(m_sbc.rcv_size);
		WAWO_ASSERT( buff_init_rt == wawo::OK );

		WAWO_LOG_DEBUG( "socket", "[#%d:%s] Socket::Socket()", m_fd, m_addr.AddressInfo() );
	}
	Socket::Socket( SocketAddr const& addr,Socket::Type const& type, Socket::Protocol const& proto, Socket::Family const& family, Socket::Option const& option  ):
		m_sm(SM_NONE),

		m_family(family),
		m_type(type),
		m_proto(proto),
		m_option(option),
		m_addr(addr),

		m_fd(FD_INIT),
		m_ec(0),
		m_state(STATE_IDLE),
		m_shutdown_flag(SSHUT_NONE),
		m_sbc( BufferConfigs[SBCT_MEDIUM] ),
		m_is_connecting(false)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] Socket::Socket(), dummy socket", m_fd, m_addr.AddressInfo() );
	}

	Socket::Socket( SocketAddr const& addr,SockBufferConfig const& sbc, Socket::Type const& type, Socket::Protocol const& proto, Socket::Family const& family, Socket::Option const& option  ):
		m_sm(SM_NONE),

		m_family(family),
		m_type(type),
		m_proto(proto),
		m_option(option),
		m_addr(addr),

		m_fd(FD_INIT),
		m_ec(0),
		m_state(STATE_IDLE),
		m_shutdown_flag(SSHUT_NONE),
		m_sbc(sbc),
		m_is_connecting(false)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] Socket::Socket(), dummy socket", m_fd, m_addr.AddressInfo() );
	}
	Socket::Socket( SockBufferConfig const& sbc , Socket::Type const& type, Socket::Protocol const& proto,Socket::Family const& family , Socket::Option const& option ):
		m_sm(SM_NONE),

		m_family(family),
		m_type(type),
		m_proto(proto),
		m_option(option),

		m_fd(FD_INIT),
		m_ec(0),
		m_state(STATE_IDLE),
		m_shutdown_flag(SSHUT_NONE),
		m_sbc(sbc),
		m_is_connecting(false)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] Socket::Socket(), dummy socket", m_fd, m_addr.AddressInfo() );
	}
	Socket::~Socket() {
		Close();
		WAWO_ASSERT( m_state == STATE_RDWR_CLOSED || m_state == STATE_IDLE );
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] Socket::~Socket,address: 0x%p", m_fd, m_addr.AddressInfo(), this );
	}

	SocketAddr const& Socket::GetAddr() const {
		return m_addr;
	}

	int Socket::Open() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_state == STATE_IDLE );
		WAWO_ASSERT( m_fd == FD_INIT );

		int soFamily;
		int soType;
		int soProtocol;

		if( m_family == FAMILY_PF_INET ) {
			soFamily =  PF_INET;
		} else if( m_family == FAMILY_AF_INET ) {
			soFamily =  AF_INET;
		} else if( m_family == FAMILY_AF_UNIX ) {
			soFamily =  AF_UNIX;
		} else {
			m_ec = wawo::E_SOCKET_INVALID_FAMILY ;
			return m_ec;
		}

		if( m_type == TYPE_STREAM ) {
			soType = SOCK_STREAM ;
		} else if( m_type == TYPE_DGRAM ) {
			soType = SOCK_DGRAM ;
		} else {
			m_ec = wawo::E_SOCKET_INVALID_TYPE ;
			WAWO_ASSERT( "invalid socket type" );
			return m_ec;
		}

		soProtocol = 0;
		m_fd = socket ( soFamily, soType, soProtocol );
		WAWO_LOG_DEBUG("socket","[#%d:%s] new socket fd create", m_fd, m_addr.AddressInfo() );

		if( m_fd == FD_ERROR ) {
			WAWO_LOG_FATAL( "socket", "[#%d:%s] Socket::socket() failed, %d", m_fd, m_addr.AddressInfo(), WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) );
			return wawo::E_SOCKET_INVALID_FD ;
		}

		//int rtl = SetLinger(true,10);
		//WAWO_ASSERT( rtl == wawo::OK );

		int rt = SetOptions( m_option );

		if( rt != wawo::OK ) {
			WAWO_LOG_FATAL( "socket", "[#%d:%s] Socket::SetOptions() failed", m_fd, m_addr.AddressInfo() );
			return rt;
		}

//		int socket_buffer_size = 1024*64;
#ifdef _DEBUG
		int tmp_size ;
		GetSndBufferSize(tmp_size);
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] current snd buffer size: %d", m_fd, m_addr.AddressInfo(), tmp_size );
#endif
		rt = SetSndBufferSize( m_sbc.snd_size );
		if( rt != wawo::OK ) {
			WAWO_LOG_FATAL( "socket", "[#%d:%s] Socket::SetSndBufferSize(%d) failed", m_fd, m_addr.AddressInfo(), m_sbc.snd_size ,  WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) );
			return rt;
		}
#ifdef _DEBUG
		GetSndBufferSize(tmp_size);
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] current snd buffer size: %d", m_fd, m_addr.AddressInfo(), tmp_size );
#endif

#ifdef _DEBUG
 		GetRcvBufferSize(tmp_size);
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] current rcv buffer size: %d", m_fd, m_addr.AddressInfo(), tmp_size );
#endif
		rt = SetRcvBufferSize( m_sbc.rcv_size );
		if( rt != wawo::OK ) {
			WAWO_LOG_FATAL( "socket", "[#%d:%s] Socket::SetRcvBufferSize(%d) failed", m_fd, m_addr.AddressInfo(),m_sbc.rcv_size,  WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) );
			return rt;
		}
#ifdef _DEBUG
		GetRcvBufferSize(tmp_size);
		WAWO_LOG_DEBUG( "socket", "[#%d:%s] current rcv buffer size: %d", m_fd, m_addr.AddressInfo(), tmp_size );
#endif
		m_state = Socket::STATE_OPENED ;
		return wawo::OK ;
	}

	int Socket::Close(int const& ec ) {
		LockGuard<SpinMutex> lg( m_mutexes[L_SOCKET]);
		if( m_fd > 0 ) {
			int close_rt = WAWO_CLOSE_SOCKET( m_fd ) ;
			WAWO_LOG_DEBUG("socket", "[#%d:%s] socket close, close_rt: %d, error_code: %d", m_fd, m_addr.AddressInfo(), close_rt, WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ) ;
			m_state = STATE_RDWR_CLOSED;
			m_shutdown_flag = Socket::SSHUT_RDWR;
			m_fd = FD_CLOSED;
			m_ec = ec;
			return close_rt;
		}
		return wawo::OK;
	}

	int Socket::_Shutdown(int const& flag, int const& ec ) {

		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( flag == SSHUT_RD ||
					 flag == SSHUT_WR ||
					 flag == SSHUT_RDWR
			);

		if( m_fd < 0 ) {
			return wawo::OK;
		}

		WAWO_ASSERT(
					 m_state == Socket::STATE_CONNECTED ||
					 m_state == Socket::STATE_RD_CLOSED ||
					 m_state == Socket::STATE_WR_CLOSED
					);


		m_ec = ec;
		int _new_shut_flag = ((~m_shutdown_flag)&flag);

		if( _new_shut_flag == 0 ) {
			WAWO_LOG_WARN("socket", "[#%d:%s] to shutdown for(%d), current shut_flag: %d, incoming shut_flag: %d, ignore !!!", m_fd, m_addr.AddressInfo(), ec, m_shutdown_flag, flag ) ;
			return wawo::OK;
		}

		m_shutdown_flag |= _new_shut_flag;
		int _shut_flag;
		if( _new_shut_flag == SSHUT_RDWR ) {
			_shut_flag = SHUT_RDWR;
		} else if( _new_shut_flag == SSHUT_RD ) {
			_shut_flag = SHUT_RD;
		} else if( _new_shut_flag == SSHUT_WR ){
			_shut_flag = SHUT_WR;
		} else {
			WAWO_THROW_EXCEPTION("what!!!, invalid shut flag");
		}

		int shut_rt = shutdown( m_fd, _shut_flag );
		WAWO_LOG_WARN("socket", "[#%d:%s] shutdown for(%d), shut flag: %d, shut_rt: %d, shut ec (only make sense for rt==-1): %d", m_fd, m_addr.AddressInfo(), ec, _shut_flag, shut_rt , WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ) ;

		//update shut_flag
		if( m_shutdown_flag==Socket::SSHUT_RDWR ) {
			m_state = STATE_RDWR_CLOSED;
		} else if(m_shutdown_flag==Socket::SSHUT_RD) {
			m_state = STATE_RD_CLOSED;
		} else if(m_shutdown_flag==Socket::SSHUT_WR) {
			m_state = STATE_WR_CLOSED;
		} else {}

		return shut_rt;
	}

	int Socket::Shutdown(int const& shut_flag, int const& ec ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );
		if( (m_shutdown_flag&shut_flag) == shut_flag ) {
			return wawo::OK;
		}
		
		return _Shutdown( shut_flag, ec );
	}

	int Socket::Bind( const SocketAddr& addr ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_state == STATE_OPENED );

		sockaddr_in addr_in;

		int soFamily ;
		if( m_family == FAMILY_PF_INET ) {
			soFamily =  PF_INET;
		} else if( m_family == FAMILY_AF_INET ) {
			soFamily =  AF_INET;
		} else if( m_family == FAMILY_AF_UNIX ) {
			soFamily =  AF_UNIX;
		} else {
			return wawo::E_SOCKET_INVALID_FAMILY ;
		}

		addr_in.sin_family		= soFamily ;
		addr_in.sin_port		= addr.GetNetSequencePort() ;
		addr_in.sin_addr.s_addr = addr.GetNetSequenceUlongIp() ;

		int result = bind( m_fd, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in) );

		if( result == 0 ) {
			m_state = STATE_BINDED ;
			m_addr = addr ;
			return wawo::OK ;
		}

		m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ;
		WAWO_LOG_FATAL("socket", "[#%d:%s] socket bind error, bind failed, errno: %d", m_fd, m_addr.AddressInfo() , m_addr.AddressInfo(), m_ec );
		return m_ec;
	}

	int Socket::Listen( int const& backlog ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_sm == SM_NONE );
		WAWO_ASSERT( m_state == STATE_BINDED );
		int rt = listen( m_fd, backlog );

		if( rt == 0 ) {
			m_sm = SM_LISTENER ;
			m_state = STATE_LISTEN ;
			WAWO_LOG_DEBUG("socket", "[#%d:%s] socket listen success, addr: %s", m_fd, m_addr.AddressInfo() , m_addr.AddressInfo() );
			return wawo::OK ;
		}

		m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
		WAWO_LOG_FATAL("socket", "[#%d:%s] socket listen error, listen failed, errno: %d", m_fd, m_addr.AddressInfo(), m_addr.AddressInfo() , m_ec );
		return m_ec;
	}

	uint32_t Socket::Accept( WAWO_REF_PTR<Socket> sockets[], uint32_t const& size, int& ec_o ) {
		
		WAWO_ASSERT( m_sm == SM_LISTENER );
		WAWO_ASSERT( (m_state == STATE_LISTEN) || m_state == STATE_RDWR_CLOSED ); //we may in close status,,,

		ec_o = wawo::OK ;
		uint32_t count = 0;
		sockaddr_in addr_in;
		socklen_t addr_len = sizeof( addr_in ) ;
		SocketAddr addr;

		do {

			int fd = accept( m_fd, reinterpret_cast<sockaddr*>(&addr_in), &addr_len );

			if( -1 == fd ) {
				break;
			}

			addr.SetHostSequencePort( ntohs(addr_in.sin_port) );
			addr.SetHostSequenceUlongIp( ntohl(addr_in.sin_addr.s_addr) );

			WAWO_REF_PTR<Socket> socket(new Socket( fd, addr,SM_PASSIVE,m_sbc, m_type, m_proto, m_family , OPTION_NONE) );

#ifdef _TEST_SOCKET_SHUTDOWN

			int rt;
			char tmp[128];
			int recv_total = 0;
			do {
				rt = recv(fd, tmp, 128, 0);
				recv_total += rt;
				WAWO_LOG_DEBUG("socket", "[#%d:%s] recv rt: %d, errno: %d", m_fd, m_addr.AddressInfo() ,rt, Socket::SocketGetLastErrno() );
			} while( rt > 0 );
				WAWO_LOG_DEBUG("socket", "[#%d:%s] recv total: %d", m_fd, m_addr.AddressInfo() , recv_total );
				socket->Shutdown(Socket::SSHUT_WR);
#endif
			//if( IsNonBlocking() ) {
			//	int rt = socket->TurnOnNonBlocking();
			//	WAWO_CONDITION_CHECK( rt == wawo::OK );
			//}

			sockets[count++] = socket ;
		} while( count < size );

		if(count == size) {
			ec_o = wawo::E_SOCKET_ACCEPT_MAY_MORE;
		}

		return count;
	}

	/*
	int Socket::Connect( SocketAddr const& addr ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );
		WAWO_ASSERT( m_addr == SocketAddr::NULL_ADDR );
		WAWO_ASSERT( addr != SocketAddr::NULL_ADDR ) ;
		m_addr = addr;
		return _Connect();
	}
	*/

	int Socket::Connect() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );
		return _Connect();
	}

	int Socket::_Connect() {
		WAWO_ASSERT( m_state == STATE_OPENED );
		WAWO_ASSERT( m_sm == SM_NONE );
		WAWO_ASSERT( !m_addr.IsNullAddr() );

		int soFamily ;
		if( m_family == FAMILY_PF_INET ) {
			soFamily =  PF_INET;
		} else if( m_family == FAMILY_AF_INET ) {
			soFamily =  AF_INET;
		} else if( m_family == FAMILY_AF_UNIX ) {
			soFamily =  AF_UNIX;
		} else {
			return wawo::E_SOCKET_INVALID_FAMILY ;
		}

		sockaddr_in addr_in;

		addr_in.sin_family		= soFamily ;
		addr_in.sin_port		= m_addr.GetNetSequencePort() ;
		addr_in.sin_addr.s_addr = m_addr.GetNetSequenceUlongIp() ;

		int socklen = sizeof( addr_in );

		m_sm = SM_ACTIVE;

		int rt = connect( m_fd, reinterpret_cast<sockaddr*>(&addr_in), socklen ) ;

		if( rt == 0 ) {
			m_state = STATE_CONNECTED ;
			return wawo::OK;
		}

		WAWO_ASSERT( rt == -1 );
		int ec = SocketGetLastErrno();
		if( IsNonBlocking() && IS_ERRNO_EQUAL_WOULDBLOCK(ec) ) {
			m_state = STATE_CONNECTING;
			LockGuard<SpinMutex> lg(m_mutexes[L_WRITE]);
			m_is_connecting = true;
			return wawo::E_SOCKET_CONNECTING;
		}

		return WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec);
	}

	int Socket::TurnOffNoDelay() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( !(m_option & Socket::OPTION_NODELAY) ) {
			return wawo::OK ;
		}

		return SetOptions( (m_option & ~Socket::OPTION_NODELAY) ) ;
	}

	int Socket::TurnOnNoDelay() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( (m_option & Socket::OPTION_NODELAY) ) {
			return wawo::OK ;
		}

		return SetOptions( (m_option | Socket::OPTION_NODELAY) )  ;
	}

	int Socket::TurnOnNonBlocking() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET]);

		if ( (m_option & Socket::OPTION_NON_BLOCKING) ) {
			return wawo::OK ;
		}

		int op = SetOptions( m_option | Socket::OPTION_NON_BLOCKING );
		return op ;
	}

	int Socket::TurnOffNonBlocking() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( !(m_option & Socket::OPTION_NON_BLOCKING) ) {
			return true ;
		}

		return SetOptions( m_option & ~Socket::OPTION_NON_BLOCKING  ) ;
	}

	int Socket::SetSndBufferSize( int const& size ) {
		WAWO_ASSERT( size >= SOCK_SND_MIN_SIZE && size <= SOCK_SND_MAX_SIZE );

		WAWO_ASSERT( m_fd > 0 );
#ifdef WAWO_PLATFORM_POSIX
		int _size = size>>1;
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&(_size), sizeof(_size) );
#else
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size) );
#endif

		if( -1 == rt ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
			WAWO_LOG_FATAL("socket", "[#%d:%s]setsockopt(SO_SNDBUF) == %d failed, error code: %d", m_fd, m_addr.AddressInfo(),size, ec );

			m_ec = ec;
			return ec ;
		}

		WAWO_LOG_DEBUG("socket", "[#%d:%s]setsockopt(SO_SNDBUF) == %d success", m_fd, m_addr.AddressInfo(), size ) ;
		return wawo::OK ;
	}

	int Socket::GetSndBufferSize( int& size ) {
		WAWO_ASSERT( m_fd > 0 );

		int iBufSize = 0;
		socklen_t opt_len = sizeof(int);

		int rt = getsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*) &iBufSize, &opt_len );

		if( rt == -1 ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ;
			WAWO_LOG_FATAL("socket", "[#%d:%s]getsockopt(SO_SNDBUF) failed, error code: %d", m_fd, m_addr.AddressInfo(), ec );
			m_ec = ec;
			return ec ;
		} else {
			size = iBufSize ;
			WAWO_LOG_DEBUG("socket", "[#%d:%s]getsockopt(SO_SNDBUF) == %d", m_fd, m_addr.AddressInfo(), size );
			return wawo::OK ;
		}
	}

	int Socket::SetRcvBufferSize( int const& size ) {
		WAWO_ASSERT( size >= SOCK_RCV_MIN_SIZE && size <= SOCK_RCV_MAX_SIZE );
		WAWO_ASSERT( m_fd > 0 );

#ifdef WAWO_PLATFORM_POSIX
		int _size = size>>1;
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&(_size), sizeof(_size) );
#else
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size) );
#endif

		if( -1 == rt ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ;
			WAWO_LOG_FATAL("socket", "[#%d:%s]setsockopt(SO_RCVBUF) == %d failed, error code: %d", m_fd, m_addr.AddressInfo(),size, ec );

			m_ec = ec;
			return ec ;
		}

		WAWO_LOG_DEBUG("socket", "[%d]setsockopt(SO_RCVBUF) == %d success", m_fd, size ) ;
		return wawo::OK ;
	}

	int Socket::GetRcvBufferSize( int& size ) {
		WAWO_ASSERT( m_fd > 0 );

		int iBufSize = 0;
		socklen_t opt_len = sizeof(int);

		int rt = getsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*) &iBufSize, &opt_len );

		if( rt == -1 ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno()) ;
			WAWO_LOG_FATAL("socket", "[#%d:%s]getsockopt(SO_RCVBUF) failed, error code: %d",m_fd, m_addr.AddressInfo(), ec );

			m_ec = ec;
			return ec ;
		} else {
			size = iBufSize;
			WAWO_LOG_DEBUG("socket", "[%d]getsockopt(SO_RCVBUF) == %d", m_fd, size );
			return wawo::OK ;
		}
	}

	int Socket::GetLinger( bool& on_off, int& linger_t ) {
		WAWO_ASSERT( m_fd > 0 );

		struct linger soLinger;
		socklen_t opt_len = sizeof(soLinger);

		int rt = getsockopt( m_fd,SOL_SOCKET, SO_LINGER, (char*)&soLinger, &opt_len );

		if( rt == 0 ) {
			on_off = (soLinger.l_onoff != 0);
			linger_t = soLinger.l_linger;
		}

		return rt ;
	}

	int Socket::SetLinger( bool const& on_off, int const& linger_t /* in seconds */ ) {
		struct linger soLinger;
		WAWO_ASSERT( m_fd > 0 );

		soLinger.l_onoff = on_off;
		soLinger.l_linger = linger_t;

		return setsockopt(m_fd,SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
	}

	int Socket::SetOptions(int const& options ) {

		if( m_fd < 0 ) {
 			WAWO_LOG_WARN( "socket", "[#%d:%s] socket set options failed, m_fd not initled", m_fd, m_addr.AddressInfo() );
			return wawo::E_SOCKET_INVALID_FD ;
		}

		if( options == 0 ) {
			//do nothing
			return wawo::OK ;
		}

	#ifdef WAWO_PLATFORM_POSIX
		int optval;
	#elif defined( WAWO_PLATFORM_WIN )
		char optval;
	#else
		#error
	#endif

		int ret = -2;
		bool should_set;
		if (m_type == Socket::TYPE_DGRAM) {

			should_set = ((m_option&Socket::OPTION_BROADCAST) && ((options&Socket::OPTION_BROADCAST) ==0)) ||
						 (((m_option&Socket::OPTION_BROADCAST)==0) && ((options&Socket::OPTION_BROADCAST))) ;

			if( should_set ) {
				optval = (options & Socket::OPTION_BROADCAST) ? 1 : 0 ;
				ret = setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
				WAWO_LOG_DEBUG( "socket", "[#%d:%s] socket set Socket::OPTION_BROADCAST, optval: %d, op ret: %d : %d", m_fd, m_addr.AddressInfo(),optval, ret );

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= Socket::OPTION_BROADCAST;
					} else {
						m_option &= ~Socket::OPTION_BROADCAST;
					}
				} else {

					WAWO_ASSERT( ret == -1 ) ;
					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
					WAWO_LOG_FATAL( "socket", "[#%d:%s] socket set Socket::OPTION_BROADCAST failed, errno: %d", m_fd, m_addr.AddressInfo(), m_ec );

					return m_ec ;
				}
			}
		}

		{
			should_set = (((m_option&Socket::OPTION_REUSEADDR) && ((options&Socket::OPTION_REUSEADDR)==0)) ||
						 (((m_option&Socket::OPTION_REUSEADDR)==0) && ((options&Socket::OPTION_REUSEADDR)))) ;

			if( should_set ) {
				optval = (options & Socket::OPTION_REUSEADDR) ? 1 : 0 ;
				ret = setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
				WAWO_LOG_DEBUG( "socket", "[#%d:%s] socket set Socket::OPTION_REUSEADDR,optval: %d, op ret: %d", m_fd, m_addr.AddressInfo(), optval, ret );

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= Socket::OPTION_REUSEADDR;
					} else {
						m_option &= ~Socket::OPTION_REUSEADDR;
					}
				} else {
					WAWO_ASSERT( ret == -1 );
					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
					WAWO_LOG_FATAL( "socket", "[#%d:%s] socket set Socket::OPTION_REUSEADDR failed, errno: %d", m_fd, m_addr.AddressInfo(), m_ec );

					return m_ec ;
				}
			}
		}

		bool _nonblocking_should_set;
#ifdef WAWO_PLATFORM_POSIX

		_nonblocking_should_set = (((m_option&Socket::OPTION_NON_BLOCKING) && ((options&Socket::OPTION_NON_BLOCKING)==0)) ||
								  (((m_option&Socket::OPTION_NON_BLOCKING)==0) && ((options&Socket::OPTION_NON_BLOCKING)))) ;
		if( _nonblocking_should_set ) {
			int mode = fcntl(m_fd, F_GETFL, 0);
			optval = (options & Socket::OPTION_NON_BLOCKING) ? 1 : 0;
			if (optval == 1) {
				mode |= O_NONBLOCK;
			} else {
				mode &= ~O_NONBLOCK;
			}
			ret = fcntl(m_fd, F_SETFL, mode);
		}

#elif defined(WAWO_PLATFORM_WIN)
		// FORBIDDEN NON-BLOCKING -> SET nonBlocking to 0
		_nonblocking_should_set = (((m_option&Socket::OPTION_NON_BLOCKING) && ((options&Socket::OPTION_NON_BLOCKING)==0)) ||
								  (((m_option&Socket::OPTION_NON_BLOCKING)==0) && ((options&Socket::OPTION_NON_BLOCKING)))) ;

		if( _nonblocking_should_set ) {
			DWORD nonBlocking = (options & Socket::OPTION_NON_BLOCKING) ? 1 : 0;
			optval = (options & Socket::OPTION_NON_BLOCKING) ? 1 : 0;
			ret = ioctlsocket(m_fd, FIONBIO, &nonBlocking) ;
		}
#else
		#error
#endif

		if(_nonblocking_should_set) {
			WAWO_LOG_DEBUG( "socket", "[#%d:%s] socket set Socket::OPTION_NON_BLOCKING, opval: %d, op ret: %d", m_fd, m_addr.AddressInfo(), optval, ret );

			if( ret == 0 ) {
				if( optval == 1 ) {
					m_option |= Socket::OPTION_NON_BLOCKING;
				} else {
					m_option &= ~Socket::OPTION_NON_BLOCKING;
				}
			} else {
				WAWO_ASSERT( ret == -1 );
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
				WAWO_LOG_FATAL( "socket", "[#%d:%s] socket set Socket::OPTION_NON_BLOCKING failed, errno: %d", m_fd, m_addr.AddressInfo(), m_ec );

				return m_ec ;
			}
		}

		if ( m_type == TYPE_STREAM) {

			should_set = (((m_option&Socket::OPTION_NODELAY) && ((options&Socket::OPTION_NODELAY)==0)) ||
							(((m_option&Socket::OPTION_NODELAY)==0) && ((options&Socket::OPTION_NODELAY)))) ;

			if( should_set ) {
				optval = (options & Socket::OPTION_NODELAY) ? 1 : 0;
				ret = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
				WAWO_LOG_DEBUG( "socket", "[#%d:%s] socket set Socket::OPTION_NODELAY, optval: %d, op ret: %d", m_fd , m_addr.AddressInfo(), optval, ret );

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= Socket::OPTION_NODELAY;
					} else {
						m_option &= ~Socket::OPTION_NODELAY;
					}
				} else {
					WAWO_ASSERT( ret  == -1);

					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
					WAWO_LOG_FATAL( "socket", "[#%d:%s] socket set Socket::OPTION_NODELAY failed, errno: %d", m_fd , m_addr.AddressInfo(), m_ec );

					return m_ec ;
				}
			}
		}

		return wawo::OK ;
	}


/*
	uint32_t Socket::SendTo( const wawo::byte_t* buff, wawo::uint32_t const& byte_len, const wawo::net::core::SocketAddr& addr, int& ec_o ) {

		LockGuard<SpinMutex> _lg( m_locks[L_WRITE] );
		sockaddr_in addr_in;
		ec_o = wawo::OK;

		WAWO_ASSERT( addr.GetNetSequenceUlongIp() != 0 ) ;
		WAWO_ASSERT( addr.GetNetSequencePort() != 0 ) ;

		addr_in.sin_family		= AF_INET;
		addr_in.sin_addr.s_addr	= addr.GetNetSequenceUlongIp();
		addr_in.sin_port		= addr.GetNetSequencePort();

		int sent = sendto(m_fd, reinterpret_cast<const char*>(buff), byte_len, 0, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in) );

		if( sent == -1 ) {
			m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
			ec_o = m_ec;
			WAWO_LOG_FATAL ( "socket", "[#%d:%s]socket SendTo() == %d, failed", m_fd, m_addr.AddressInfo(), m_ec );
		} else {
			if( ((uint32_t)sent) != byte_len ) {
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(wawo::net::core::SocketGetLastErrno());
				ec_o = m_ec;
				WAWO_LOG_FATAL("socket", "[#%d:%s]Socket::SendTo() expect to send: %d, but sent: %d", m_fd, m_addr.AddressInfo(), byte_len, sent );
			}
		}

		WAWO_LOG_DEBUG("socket", "[#%d:%s]Socket::SendTo(%s) == %d", m_fd, m_addr.AddressInfo(), buff, sent );
		return sent;
	}

	
	uint32_t Socket::RecvFrom( byte_t* const buff_o, wawo::uint32_t const& size, SocketAddr& addr_o, int& ec_o ) {
		LockGuard<SpinMutex> _lg( m_locks[L_READ] );
		sockaddr_in addr_in ;
		ec_o = wawo::OK;

		socklen_t slen = sizeof(addr_in);

		int received = recvfrom( m_fd, reinterpret_cast<char*>(buff_o), size, 0, reinterpret_cast<sockaddr*>(&addr_in), &slen );

		int _ern = wawo::net::core::SocketGetLastErrno();

		if( received < 0 ) {
			if( IS_ERRNO_EQUAL_WOULDBLOCK(_ern) ) {
				ec_o = _ern;
				WAWO_LOG_DEBUG("socket", "[#%d:%s]recvfrom(%d) EWOULDBLOCK", m_fd, m_addr.AddressInfo() );
				received = 0;
			} else 	 {
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(_ern) ;
				ec_o = m_ec;
				WAWO_LOG_WARN("socket", "[#%d:%s]recvfrom(%d) ERROR: %d", m_fd, m_addr.AddressInfo(), _ern );
			}
		}

		addr_o.SetHostSequencePort( ntohs( addr_in.sin_port ) );
		addr_o.SetHostSequenceUlongIp( ntohl( addr_in.sin_addr.s_addr ) );

		return received ;
	}
	*/

	int const& Socket::GetErrorCode() const {
		return m_ec;
	}

	uint32_t Socket::Send( byte_t const* const buffer, uint32_t const& size, int& ec_o, int const& flags ) {

		if( (m_shutdown_flag&SSHUT_WR) || (m_fd < 0) ) {
			ec_o = wawo::E_SOCKET_NOT_CONNECTED ;
			return 0;
		}

		WAWO_ASSERT( buffer != NULL );
		WAWO_ASSERT( size > 0 );

		int tt = 0; //try time
		uint32_t sent_total = 0;
//		bool has_send_err = false ;
		ec_o = wawo::OK ; //no error

		int send_ec = 0;

		//TRY SEND
		do {

			int sent = send( m_fd, reinterpret_cast<const char*>(buffer) + sent_total, size-sent_total, flags );
			if( sent > 0 ) {

				WAWO_LOG_DEBUG("socket","[#%d:%s] socket::send, sent bytes: %d" , m_fd, m_addr.AddressInfo(), sent ) ;
				//ok
	#ifdef _DEBUG
				if( tt>0 ) {
					WAWO_LOG_WARN("socket","[#%d:%s] socket::send, send blocked, try success, try_time: %d" , m_fd, m_addr.AddressInfo(), tt ) ;
				}
	#endif

				tt = 0 ;
				ec_o = wawo::OK ;
				sent_total += sent;
			} else {
				//error
				if( sent == -1 ) {
					send_ec = wawo::net::core::SocketGetLastErrno();
					if( send_ec == EINTR ) {
						continue;
					} else if( IS_ERRNO_EQUAL_WOULDBLOCK(send_ec) ) {
						WAWO_ASSERT( IsNonBlocking() ); //is nonblocking, can blocking io get this error ?

						WAWO_LOG_FATAL("socket","[#%d:%s] socket::send blocked, error code: <%d>, try_time: %d" , m_fd, m_addr.AddressInfo(), send_ec, tt ) ;

						if( tt == __SEND_MAX_TRY_TIME__ ) {
							ec_o = wawo::E_SOCKET_SEND_IO_BLOCK;
							WAWO_LOG_FATAL("socket","[#%d:%s] socket::send blocked, error code: <%d>, try failed, try_time: %d" , m_fd, m_addr.AddressInfo(), send_ec, tt ) ;
							break;
						}

						//YIELD CPU RESOURCE FOR OTHER THREADS
						wawo::usleep( (1<<(tt++)) + __SEND_TRY_TIME_ADDITION_TIME__ );
					} else {
						WAWO_LOG_FATAL("socket","[#%d:%s] socket::send failed, error code: <%d>" , m_fd, m_addr.AddressInfo(), send_ec ) ;
						ec_o = WAWO_TRANSLATE_SOCKET_ERROR_CODE(send_ec) ;
						break;
					}
				}
			}
		} while( sent_total < size );

		WAWO_LOG_DEBUG("socket","[#%d:%s] socket::send bytes total: %d" , m_fd, m_addr.AddressInfo(), sent_total ) ;
		return sent_total ;
	}

	uint32_t Socket::Recv( byte_t* const buffer_o, uint32_t const& size, int& ec_o, int const& flag ) {
		WAWO_ASSERT( buffer_o != NULL );
		WAWO_ASSERT( size > 0 );

		if( (m_shutdown_flag&SSHUT_RD) || m_fd < 0 ) {
			ec_o = wawo::E_SOCKET_NOT_CONNECTED ;
			return 0;
		}

		ec_o = wawo::OK ;
		uint32_t received_total = 0;

		do {

			int received = recv( m_fd, reinterpret_cast<char*>( buffer_o) + received_total, size - received_total , flag );

			if( received > 0 ) {
				received_total += received ;
				WAWO_LOG_DEBUG("socket", "[#%d:%s] socket::recv bytes, %d", m_fd, m_addr.AddressInfo(), received );
				break;
			} else if ( received == 0 ) {
				WAWO_LOG_DEBUG("socket", "[#%d:%s] socket has been gracefully closed by remote side[detected by recv]", m_fd, m_addr.AddressInfo() );
				ec_o = wawo::E_SOCKET_REMOTE_GRACE_CLOSE;
				break;
			} else if( received == -1 ) {
				//a errno would be set
				int ec = wawo::net::core::SocketGetLastErrno();

				if( IS_ERRNO_EQUAL_WOULDBLOCK(ec) ) {
					WAWO_ASSERT( IsNonBlocking() );
					WAWO_LOG_DEBUG("socket","[#%d:%s] recv blocked, block code: <%d>" , m_fd, m_addr.AddressInfo(), ec ) ;
					ec_o = wawo::E_SOCKET_RECV_IO_BLOCK ;
					break;
				} else if( ec == EINTR ) {
					continue;
				} else {
					ec_o = WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec) ;
					WAWO_LOG_FATAL("socket", "[#%d:%s] socket::recv error, errno: %d", m_fd, m_addr.AddressInfo(), ec );
					break;
				}
			} else {
				WAWO_THROW_EXCEPTION( "[socket] socket::recv; Are u kiddng me !!!!" );
			}
		} while(true) ;

		return received_total ;
	}

#ifdef WAWO_PLATFORM_WIN
	class WinsockInit : public wawo::SafeSingleton<WinsockInit> {
	public:
		WinsockInit() {
			WSADATA __wsaData;
			int result = WSAStartup( MAKEWORD(2,2), &__wsaData );
			WAWO_CONDITION_CHECK( result == 0 );
		}
		~WinsockInit() {
			int result = WSACleanup();
			WAWO_CONDITION_CHECK( result == 0 );
		}
	};
	static WinsockInit const& __winsock_init__ = *(WinsockInit::GetInstance()) ;
#endif

}}}
