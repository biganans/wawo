#include <wawo/core.h>
#include <wawo/log/LoggerManager.h>
#include <wawo/time/time.hpp>
#include <wawo/net/Socket.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::core;

	SockBufferConfig GetBufferConfig( u32_t const& type ) {
		static SockBufferConfig BufferConfigs[SBCT_MAX] =
		{
			{((1024*1024*2)-WAWO_MALLOC_H_RESERVE),((1024*1024*2)-WAWO_MALLOC_H_RESERVE)}, //2M //super large
			{(1024*768-WAWO_MALLOC_H_RESERVE),		(1024*768-WAWO_MALLOC_H_RESERVE)}, //768K large
			{(1024*256-WAWO_MALLOC_H_RESERVE),		(1024*256-WAWO_MALLOC_H_RESERVE)}, //256k medium upper
			{1024*96,		1024*96}, //96k medium
			{1024*32,		1024*32}, //32k tiny
			{1024*4,		1024*4}, //4k, super tiny
		};

		WAWO_ASSERT( type < SBCT_MAX );
		return BufferConfigs[type];
	}

	SocketBase::SocketBase( int const& fd, SocketAddr const& addr, SocketMode const& sm , SockBufferConfig const& sbc, Family const& family, SockT const& sockt, Protocol const& proto , Option const& option ):
		m_sm(sm),
		m_family(family),
		m_sockt(sockt),
		m_protocol(proto),
		m_option(option),

		m_addr(addr),
		m_fd(fd),
		m_ec(wawo::OK),
		m_state( S_ACCEPTED ),
		m_shutdown_flag(SHUTDOWN_NONE),

		m_sbc(sbc),
		m_is_connecting(false),
		m_rs(S_READ_IDLE),
		m_ws(S_WRITE_IDLE ),
		m_tlp(NULL)
	{
		WAWO_ASSERT( fd > 0 );

		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );

		int buff_init_rt = SetSndBufferSize( m_sbc.snd_size );
		WAWO_ASSERT( buff_init_rt == wawo::OK );

		buff_init_rt = SetRcvBufferSize(m_sbc.rcv_size);
		WAWO_ASSERT( buff_init_rt == wawo::OK );

		(void)buff_init_rt;
		WAWO_DEBUG( "[socket][#%d:%s] Socket::Socket(), address: %p", m_fd, m_addr.AddressInfo().CStr(), this );
	}

	SocketBase::SocketBase( Family const& family, SockT const& sockt, Protocol const& proto,Option const& option ) :
		m_sm(SM_NONE),

		m_family(family),
		m_sockt(sockt),
		m_protocol(proto),
		m_option(option),
		m_addr(),

		m_fd(FD_NONECONN),
		m_ec(0),
		m_state(S_IDLE),
		m_shutdown_flag(SHUTDOWN_NONE),
		m_sbc( GetBufferConfig(SBCT_MEDIUM) ),
		m_is_connecting(false),
		m_rs(S_READ_IDLE),
		m_ws(S_WRITE_IDLE ),
		m_tlp(NULL)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_DEBUG( "[socket][#%d:%s] Socket::Socket(), dummy socket, address: %p", m_fd, m_addr.AddressInfo().CStr(), this );
	}

	SocketBase::SocketBase( SocketAddr const& addr, Family const& family,SockT const& sockt, Protocol const& proto, Option const& option  ):
		m_sm(SM_NONE),

		m_family(family),
		m_sockt(sockt),
		m_protocol(proto),
		m_option(option),
		m_addr(addr),

		m_fd(FD_NONECONN),
		m_ec(0),
		m_state(S_IDLE),
		m_shutdown_flag(SHUTDOWN_NONE),
		m_sbc( GetBufferConfig(SBCT_MEDIUM) ),
		m_is_connecting(false),
		m_rs(S_READ_IDLE),
		m_ws(S_WRITE_IDLE ),
		m_tlp(NULL)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_DEBUG( "[socket][#%d:%s] Socket::Socket(), dummy socket, address: %p", m_fd, m_addr.AddressInfo().CStr(),this );
	}

	SocketBase::SocketBase( SocketAddr const& addr,SockBufferConfig const& sbc, Family const& family, SockT const& sockt, Protocol const& proto, Option const& option  ):
		m_sm(SM_NONE),

		m_family(family),
		m_sockt(sockt),
		m_protocol(proto),
		m_option(option),
		m_addr(addr),

		m_fd(FD_NONECONN),
		m_ec(0),
		m_state(S_IDLE),
		m_shutdown_flag(SHUTDOWN_NONE),
		m_sbc(sbc),
		m_is_connecting(false),
		m_rs(S_READ_IDLE),
		m_ws(S_WRITE_IDLE ),
		m_tlp(NULL)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_DEBUG( "[socket][#%d:%s] Socket::Socket(), dummy socket, address: %p", m_fd, m_addr.AddressInfo().CStr(), this );
	}
	SocketBase::SocketBase( SockBufferConfig const& sbc ,Family const& family, SockT const& sockt, Protocol const& proto , Option const& option ):
		m_sm(SM_NONE),

		m_family(family),
		m_sockt(sockt),
		m_protocol(proto),
		m_option(option),
		m_addr(),

		m_fd(FD_NONECONN),
		m_ec(0),
		m_state(S_IDLE),
		m_shutdown_flag(SHUTDOWN_NONE),
		m_sbc(sbc),
		m_is_connecting(false),
		m_rs(S_READ_IDLE),
		m_ws(S_WRITE_IDLE ),
		m_tlp(NULL)
	{
		WAWO_ASSERT( m_sbc.rcv_size <= SOCK_RCV_MAX_SIZE && m_sbc.rcv_size >= SOCK_RCV_MIN_SIZE );
		WAWO_ASSERT( m_sbc.snd_size <= SOCK_SND_MAX_SIZE && m_sbc.snd_size >= SOCK_SND_MIN_SIZE );
		WAWO_DEBUG( "[socket][#%d:%s] Socket::Socket(), dummy socket, address: %p", m_fd, m_addr.AddressInfo().CStr(), this );
	}
	SocketBase::~SocketBase() {
		_Close();
		WAWO_DEBUG( "[socket][#%d:%s] Socket::~Socket(),address: %p", m_fd, m_addr.AddressInfo().CStr(), this );
	}

	SocketAddr SocketBase::GetLocalAddr() const {

		if ( IsListener() ) {
			return m_addr;
		}

		struct sockaddr_in addr_in;
		socklen_t addr_in_len = sizeof(addr_in);

		if( m_fd<0 ) { return SocketAddr(); }
		int rt = getsockname( m_fd, (struct sockaddr*) &addr_in, &addr_in_len );
		if( rt != 0 ) {
			return SocketAddr();
		}

		WAWO_ASSERT( addr_in.sin_family == AF_INET );
		return SocketAddr( addr_in );
	}

	int SocketBase::Open() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_state == S_IDLE );
		WAWO_ASSERT( m_fd == FD_NONECONN );

		int soFamily;
		int soType;
		int soProtocol;

		if( m_family == F_PF_INET ) {
			soFamily =  PF_INET;
		} else if( m_family == F_AF_INET ) {
			soFamily =  AF_INET;
		} else if( m_family == F_AF_UNIX ) {
			soFamily =  AF_UNIX;
		} else {
			m_ec = wawo::E_SOCKET_INVALID_FAMILY ;
			return m_ec;
		}

		if( m_sockt == ST_STREAM ) {
			soType = SOCK_STREAM ;
		} else if( m_sockt == ST_DGRAM ) {
			soType = SOCK_DGRAM ;
		} else {
			m_ec = wawo::E_SOCKET_INVALID_TYPE ;
			WAWO_ASSERT( "invalid socket type" );
			return m_ec;
		}

		soProtocol = 0;
		int fd = socket ( soFamily, soType, soProtocol );
		WAWO_DEBUG("[socket][#%d:%s] new socket fd create", m_fd, m_addr.AddressInfo().CStr() );

		if( fd == -1 ) {
			m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
			WAWO_FATAL( "[socket][#%d:%s] Socket::socket() failed, %d", m_fd, m_addr.AddressInfo().CStr(), m_ec );
			return m_ec ;
		}

		m_fd = fd;
		int rt = SetOptions( m_option );

		if( rt != wawo::OK ) {
			WAWO_FATAL( "[socket][#%d:%s] Socket::SetOptions() failed", m_fd, m_addr.AddressInfo().CStr() );
			return rt;
		}

//		int socket_buffer_size = 1024*64;
#ifdef _DEBUG
		u32_t tmp_size ;
		GetSndBufferSize(tmp_size);
		WAWO_DEBUG( "[socket][#%d:%s] current snd buffer size: %d", m_fd, m_addr.AddressInfo().CStr(), tmp_size );
#endif
		rt = SetSndBufferSize( m_sbc.snd_size );
		if( rt != wawo::OK ) {
			WAWO_FATAL( "[socket][#%d:%s] Socket::SetSndBufferSize(%d) failed", m_fd, m_addr.AddressInfo().CStr(), m_sbc.snd_size ,  WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) );
			return rt;
		}
#ifdef _DEBUG
		GetSndBufferSize(tmp_size);
		WAWO_DEBUG( "[socket][#%d:%s] current snd buffer size: %d", m_fd, m_addr.AddressInfo().CStr(), tmp_size );
#endif

#ifdef _DEBUG
 		GetRcvBufferSize(tmp_size);
		WAWO_DEBUG( "[socket][#%d:%s] current rcv buffer size: %d", m_fd, m_addr.AddressInfo().CStr(), tmp_size );
#endif
		rt = SetRcvBufferSize( m_sbc.rcv_size );
		if( rt != wawo::OK ) {
			WAWO_FATAL( "[socket][#%d:%s] Socket::SetRcvBufferSize(%d) failed", m_fd, m_addr.AddressInfo().CStr(),m_sbc.rcv_size,  WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) );
			return rt;
		}
#ifdef _DEBUG
		GetRcvBufferSize(tmp_size);
		WAWO_DEBUG( "[socket][#%d:%s] current rcv buffer size: %d", m_fd, m_addr.AddressInfo().CStr(), tmp_size );
#endif
		m_state = S_OPENED ;
		return wawo::OK ;
	}

	int SocketBase::Close(int const& ec ) {
		LockGuard<SpinMutex> lg( m_mutexes[L_SOCKET]);
		return _Close(ec);
	}

	int SocketBase::_Close(int const& ec ) {

		if( m_fd < 0 ) { return wawo::E_SOCKET_INVALID_OPERATION;}

		WAWO_ASSERT( m_state != S_CLOSED );
		int close_rt = WAWO_CLOSE_SOCKET( m_fd ) ;
		WAWO_ASSERT( close_rt == wawo::OK );

		if( IsDataSocket() ) {
			m_shutdown_flag = SHUTDOWN_RDWR;
		}

		m_fd = FD_CLOSED;
		m_ec = ec;
		m_state = S_CLOSED;

		if( close_rt == 0 ) {
			WAWO_DEBUG("[socket][#%d:%s] socket close, for reason: %d", m_fd, m_addr.AddressInfo().CStr(), ec ) ;
		} else {
			WAWO_WARN("[socket][#%d:%s] socket close, for reason: %d, close_rt: %d, close_ec: %d", m_fd, m_addr.AddressInfo().CStr(), ec, close_rt, WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ) ;
		}

		return close_rt;
	}

	int SocketBase::_Shutdown(int const& flag, int const& ec ) {

		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( flag == SHUTDOWN_RD ||
					 flag == SHUTDOWN_WR ||
					 flag == SHUTDOWN_RDWR
			);

		if( m_fd < 0 ) { return wawo::E_SOCKET_INVALID_OPERATION;}
		int new_flag = ((~m_shutdown_flag)&flag);
		if( new_flag == 0 ) {
			WAWO_DEBUG("[socket][#%d:%s] shutdown for(%d), current shut_flag: %d, incoming shut_flag: %d, ignore !!!", m_fd, m_addr.AddressInfo().CStr(), ec, m_shutdown_flag, flag ) ;
			return wawo::E_SOCKET_INVALID_OPERATION;
		}

		int shutdown_flag;
		if( new_flag == SHUTDOWN_RD ) {
			shutdown_flag = SHUT_RD;
		} else if( new_flag == SHUTDOWN_WR ){
			shutdown_flag = SHUT_WR;
		} else if (new_flag == SHUTDOWN_RDWR) {
			shutdown_flag = SHUT_RDWR;
		} else {
			WAWO_THROW_EXCEPTION("what!!!, invalid shut flag");
		}

		const char* shutdown_flag_str[3] = {
			"SHUT_RD",
			"SHUT_WR",
			"SHUT_RDWR"
		};

		int shutdown_rt = shutdown( m_fd, shutdown_flag );
		if( shutdown_rt == 0 ) {
			WAWO_DEBUG("[socket][#%d:%s] shutdown(%s) for(%d)", m_fd, m_addr.AddressInfo().CStr(), shutdown_flag_str[shutdown_flag], ec ) ;
		} else {
			WAWO_WARN("[socket][#%d:%s] shutdown(%s) for (%d), shut_rt: %d, shutdown ec (only make sense for shut_rt==-1): %d", m_fd, m_addr.AddressInfo().CStr(), shutdown_flag_str[shutdown_flag], ec, shutdown_rt , WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ) ;
		}
		m_shutdown_flag |= new_flag;
		m_ec = ec;
		return shutdown_rt;
	}

	int SocketBase::Shutdown(int const& shut_flag, int const& ec ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if( (m_shutdown_flag&shut_flag) == shut_flag ) {
			return wawo::E_SOCKET_INVALID_OPERATION;
		}

		return SocketBase::_Shutdown( shut_flag, ec );
	}

	int SocketBase::Bind( const SocketAddr& addr ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_state == S_OPENED );
		WAWO_ASSERT( m_sm == SM_NONE );

		sockaddr_in addr_in;

		int soFamily ;
		if( m_family == F_PF_INET ) {
			soFamily =  PF_INET;
		} else if( m_family == F_AF_INET ) {
			soFamily =  AF_INET;
		} else if( m_family == F_AF_UNIX ) {
			soFamily =  AF_UNIX;
		} else {
			return wawo::E_SOCKET_INVALID_FAMILY ;
		}

		addr_in.sin_family		= soFamily ;
		addr_in.sin_port		= addr.GetNetSequencePort() ;
		addr_in.sin_addr.s_addr = addr.GetNetSequenceUlongIp() ;

		int result = bind( m_fd, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in) );

		if( result == 0 ) {
			m_sm = SM_LISTENER ;
			m_state = S_BINDED ;
			m_addr = addr ;
			return wawo::OK ;
		}

		m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ;
		WAWO_FATAL("[socket][#%d:%s] socket bind error, bind failed, errno: %d", m_fd, m_addr.AddressInfo().CStr() , m_addr.AddressInfo().CStr(), m_ec );
		return m_ec;
	}

	int SocketBase::Listen( int const& backlog ) {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		WAWO_ASSERT( m_sm == SM_LISTENER );
		WAWO_ASSERT( m_state == S_BINDED );
		int rt = listen( m_fd, backlog );

		if( rt == 0 ) {
			m_state = S_LISTEN ;
			WAWO_DEBUG("[socket][#%d:%s] socket listen success, addr: %s", m_fd, m_addr.AddressInfo().CStr() , m_addr.AddressInfo().CStr() );
			return wawo::OK ;
		}

		m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
		WAWO_FATAL("[socket][#%d:%s] socket listen error, listen failed, errno: %d", m_fd, m_addr.AddressInfo().CStr(), m_addr.AddressInfo().CStr() , m_ec );
		return m_ec;
	}

	u32_t SocketBase::Accept( WWRP<SocketBase> sockets[], u32_t const& size, int& ec_o ) {

		WAWO_ASSERT( m_sm == SM_LISTENER );
		if( m_state != S_LISTEN ) {
			ec_o = wawo::E_SOCKET_INVALID_STATE;
			return 0;
		}

		ec_o = wawo::OK ;
		u32_t count = 0;
		sockaddr_in addr_in;
		socklen_t addr_len = sizeof( addr_in ) ;

		LockGuard<SpinMutex> lg( m_mutexes[L_READ] );
		WAWO_ASSERT( m_rs == S_READ_POSTED || m_rs == S_READ_CONTINUE);
		m_rs = S_READING;
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

			addr.SetNetSequencePort( (addr_in.sin_port) );
			addr.SetNetSequenceUlongIp( (addr_in.sin_addr.s_addr) );

			WWRP<SocketBase> socket(new SocketBase( fd, addr,SM_PASSIVE,m_sbc,m_family, m_sockt, m_protocol, OPTION_NONE) );
			sockets[count++] = socket ;
		} while( count < size );

		if(count == size) {
			ec_o = wawo::E_TRY_AGAIN;
		}

		m_rs = (ec_o == wawo::E_TRY_AGAIN) ? S_READ_CONTINUE : S_READ_IDLE;

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

	int SocketBase::Connect() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );
		if( m_state == S_OPENED ) {
			return _Connect();
		} else {
			return wawo::E_SOCKET_INVALID_STATE;
		}
	}

	int SocketBase::AsyncConnect() {
		int rt = TurnOnNonBlocking();
		if( rt != wawo::OK ) {
			return rt;
		}
		return Connect();
	}

	void SocketBase::HandlePassiveConnected(int& ec_o) {
		LockGuard<SpinMutex> _lg(m_mutexes[L_SOCKET]);
		ec_o = wawo::OK;
		WAWO_ASSERT( m_state == S_ACCEPTED );
		if (m_state != S_ACCEPTED) {
			WAWO_ASSERT(m_fd < 0);
			ec_o = wawo::E_SOCKET_INVALID_STATE;
			return;
		}
		WAWO_ASSERT(m_fd > 0);
		WAWO_ASSERT(m_sm == SM_PASSIVE);
		m_state = S_CONNECTED;
	}

	void SocketBase::HandleAsyncConnected(int& ec_o) {
		LockGuard<SpinMutex> _lgw( m_mutexes[L_WRITE] );

		WAWO_ASSERT( m_ws == S_WRITE_POSTED );
		m_ws = S_WRITING;

		LockGuard<SpinMutex> _lg(m_mutexes[L_SOCKET]);
		ec_o = wawo::OK;

		if( m_state == S_CLOSED ) {
			WAWO_ASSERT( m_fd < 0 );
			ec_o = wawo::E_SOCKET_INVALID_STATE;
			m_ws = S_WRITE_IDLE;
			return ;
		}

		WAWO_ASSERT( m_fd > 0 );
		WAWO_ASSERT( m_sm == SM_ACTIVE );
		WAWO_ASSERT( IsNonBlocking() );

		WAWO_ASSERT( m_state == S_CONNECTING );
		WAWO_ASSERT( m_is_connecting );

		int iError = 0;
		socklen_t opt_len = sizeof(int);

		if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (char*)&iError, &opt_len) < 0) {
			WAWO_FATAL( "[socket][#%d:%s]getsockopt failed: %d",m_fd,m_addr.AddressInfo().CStr(), iError );
		}

		if( iError == 0 ) {
			WAWO_DEBUG("[socket_proxy]socket connected, local addr: %s, remote addr: %s", GetLocalAddr().AddressInfo().CStr(), GetRemoteAddr().AddressInfo().CStr());
			m_state = S_CONNECTED ;
			m_is_connecting = false;
		}
		m_ws = S_WRITE_IDLE;
	}

	int SocketBase::_Connect() {
		WAWO_ASSERT( m_state == S_OPENED );
		WAWO_ASSERT( m_sm == SM_NONE );
		WAWO_ASSERT( !m_addr.IsNullAddr() );

		int soFamily ;
		if( m_family == F_PF_INET ) {
			soFamily = PF_INET;
		} else if( m_family == F_AF_INET ) {
			soFamily = AF_INET;
		} else if( m_family == F_AF_UNIX ) {
			soFamily = AF_UNIX;
		} else {
			return wawo::E_SOCKET_INVALID_FAMILY ;
		}

		m_sm = SM_ACTIVE;
		sockaddr_in addr_in;
		addr_in.sin_family		= soFamily ;
		addr_in.sin_port		= m_addr.GetNetSequencePort() ;
		addr_in.sin_addr.s_addr = m_addr.GetNetSequenceUlongIp() ;
		int socklen = sizeof( addr_in );

		int rt = connect( m_fd, reinterpret_cast<sockaddr*>(&addr_in), socklen ) ;
		if( rt == 0 ) {
			WAWO_DEBUG("[socket]socket connected, local addr: %s, remote addr: %s", GetLocalAddr().AddressInfo().CStr(), GetRemoteAddr().AddressInfo().CStr());
			m_state = S_CONNECTED ;
			return wawo::OK;
		}

		WAWO_ASSERT( rt == -1 );
		int ec = SocketGetLastErrno();
		if( IsNonBlocking() && (IS_ERRNO_EQUAL_CONNECTING(ec)) ) {
			m_state = S_CONNECTING;
			LockGuard<SpinMutex> lg(m_mutexes[L_WRITE]);
			m_is_connecting = true;
			return wawo::E_SOCKET_CONNECTING;
		}

		return WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec);
	}

	int SocketBase::TurnOffNoDelay() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( !(m_option & OPTION_NODELAY) ) {
			return wawo::OK ;
		}

		return SetOptions( (m_option & ~OPTION_NODELAY) ) ;
	}

	int SocketBase::TurnOnNoDelay() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( (m_option & OPTION_NODELAY) ) {
			return wawo::OK ;
		}

		return SetOptions( (m_option | OPTION_NODELAY) )  ;
	}

	int SocketBase::TurnOnNonBlocking() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET]);

		if ( (m_option & OPTION_NON_BLOCKING) ) {
			return wawo::OK ;
		}

		int op = SetOptions( m_option | OPTION_NON_BLOCKING );
		return op ;
	}

	int SocketBase::TurnOffNonBlocking() {
		LockGuard<SpinMutex> _lg( m_mutexes[L_SOCKET] );

		if ( !(m_option & OPTION_NON_BLOCKING) ) {
			return true ;
		}

		return SetOptions( m_option & ~OPTION_NON_BLOCKING  ) ;
	}

	int SocketBase::SetSndBufferSize( u32_t const& size ) {
		WAWO_ASSERT( size >= SOCK_SND_MIN_SIZE && size <= SOCK_SND_MAX_SIZE );
		WAWO_ASSERT( m_fd > 0 );

#ifdef WAWO_PLATFORM_GNU
		int _size = size>>1;
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&(_size), sizeof(_size) );
#else
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&(size), sizeof(size) );
#endif

		if( -1 == rt ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
			WAWO_FATAL("[socket][#%d:%s]setsockopt(SO_SNDBUF) == %d failed, error code: %d", m_fd, m_addr.AddressInfo().CStr(),size, ec );
			m_ec = ec;
			return ec ;
		}

		return wawo::OK ;
	}

	int SocketBase::GetSndBufferSize(u32_t& size ) const {
		WAWO_ASSERT( m_fd > 0 );

		int iBufSize = 0;
		socklen_t opt_len = sizeof(u32_t);
		int rt = getsockopt( m_fd, SOL_SOCKET, SO_SNDBUF, (char*) &iBufSize, &opt_len );

		if( rt == -1 ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ;
			WAWO_FATAL("[socket][#%d:%s]getsockopt(SO_SNDBUF) failed, error code: %d", m_fd, m_addr.AddressInfo().CStr(), ec );
			return ec ;
		} else {
			size = iBufSize ;
			return wawo::OK ;
		}
	}

	int SocketBase::GetLeftSndQueue(u32_t& size) const {
#if WAWO_ISGNU
		if( m_fd <= 0 ) {
			size = 0;
			return -1;
		}

        int rt= ioctl(m_fd, TIOCOUTQ, &size);

        WAWO_RETURN_V_IF_MATCH(rt,rt==0);
        return SocketGetLastErrno();
#else
		WAWO_THROW_EXCEPTION("this operation does not supported on windows");
#endif
	}

	int SocketBase::GetLeftRcvQueue(u32_t& size) const {
		if(m_fd<=0) {
			size=0;
			return -1;
		}
#if WAWO_ISGNU
		int rt = ioctl(m_fd, FIONREAD, size);
#else
		u_long ulsize;
		int rt = ioctlsocket(m_fd, FIONREAD, &ulsize );
		if (rt == 0) size = ulsize & 0xFFFFFFFF;
#endif

		WAWO_RETURN_V_IF_MATCH(rt, rt==0);
		return SocketGetLastErrno();
	}

	int SocketBase::SetRcvBufferSize( u32_t const& size ) {
		WAWO_ASSERT( size >= SOCK_RCV_MIN_SIZE && size <= SOCK_RCV_MAX_SIZE );
		WAWO_ASSERT( m_fd > 0 );

#ifdef WAWO_PLATFORM_GNU
		int _size = size>>1;
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&(_size), sizeof(_size) );
#else
		int rt = setsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&(size), sizeof(size) );
#endif

		if( -1 == rt ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ;
			WAWO_FATAL("[socket][#%d:%s]setsockopt(SO_RCVBUF) == %d failed, error code: %d", m_fd, m_addr.AddressInfo().CStr(),size, ec );

			m_ec = ec;
			return ec ;
		}

		return wawo::OK ;
	}

	int SocketBase::GetRcvBufferSize( u32_t& size ) const {
		WAWO_ASSERT( m_fd > 0 );

		int iBufSize = 0;
		socklen_t opt_len = sizeof(u32_t);

		int rt = getsockopt( m_fd, SOL_SOCKET, SO_RCVBUF, (char*) &iBufSize, &opt_len );

		if( rt == -1 ) {
			int ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno()) ;
			WAWO_FATAL("[socket][#%d:%s]getsockopt(SO_RCVBUF) failed, error code: %d",m_fd, m_addr.AddressInfo().CStr(), ec );

			return ec ;
		} else {
			size = iBufSize;
			return wawo::OK ;
		}
	}

	int SocketBase::GetLinger( bool& on_off, int& linger_t ) const {
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

	int SocketBase::SetLinger( bool const& on_off, int const& linger_t /* in seconds */ ) {
		struct linger soLinger;
		WAWO_ASSERT( m_fd > 0 );

		soLinger.l_onoff = on_off;
		soLinger.l_linger = linger_t;

		return setsockopt(m_fd,SOL_SOCKET, SO_LINGER, (char*)&soLinger, sizeof(soLinger));
	}

	int SocketBase::SetOptions(int const& options ) {

		if( m_fd < 0 ) {
 			WAWO_WARN( "[socket][#%d:%s] socket set options failed, m_fd not initled", m_fd, m_addr.AddressInfo().CStr() );
			return wawo::E_SOCKET_INVALID_FD ;
		}

	#if WAWO_ISGNU
		int optval;
	#elif WAWO_ISWIN
		char optval;
	#else
		#error
	#endif

		int ret = -2;
		bool should_set;
		if (m_sockt == ST_DGRAM) {

			should_set = ((m_option&OPTION_BROADCAST) && ((options&OPTION_BROADCAST) ==0)) ||
						 (((m_option&OPTION_BROADCAST)==0) && ((options&OPTION_BROADCAST))) ;

			if( should_set ) {
				optval = (options & OPTION_BROADCAST) ? 1 : 0 ;
				ret = setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= OPTION_BROADCAST;
					} else {
						m_option &= ~OPTION_BROADCAST;
					}
				} else {

					WAWO_ASSERT( ret == -1 ) ;
					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
					WAWO_FATAL( "[socket][#%d:%s] socket set Socket::OPTION_BROADCAST failed, errno: %d", m_fd, m_addr.AddressInfo().CStr(), m_ec );

					return m_ec ;
				}
			}
		}

		{
			should_set = (((m_option&OPTION_REUSEADDR) && ((options&OPTION_REUSEADDR)==0)) ||
						 (((m_option&OPTION_REUSEADDR)==0) && ((options&OPTION_REUSEADDR)))) ;

			if( should_set ) {
				optval = (options & OPTION_REUSEADDR) ? 1 : 0 ;
				ret = setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= OPTION_REUSEADDR;
					} else {
						m_option &= ~OPTION_REUSEADDR;
					}
				} else {
					WAWO_ASSERT( ret == -1 );
					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
					WAWO_FATAL( "[socket][#%d:%s] socket set Socket::OPTION_REUSEADDR failed, errno: %d", m_fd, m_addr.AddressInfo().CStr(), m_ec );

					return m_ec ;
				}
			}
		}

		bool _nonblocking_should_set = (((m_option&OPTION_NON_BLOCKING) && ((options&OPTION_NON_BLOCKING) == 0)) ||
										(((m_option&OPTION_NON_BLOCKING) == 0) && ((options&OPTION_NON_BLOCKING))));
#if WAWO_ISGNU
		if( _nonblocking_should_set ) {
			int mode = fcntl(m_fd, F_GETFL, 0);
			optval = (options & OPTION_NON_BLOCKING) ? 1 : 0;
			if (optval == 1) {
				mode |= O_NONBLOCK;
			} else {
				mode &= ~O_NONBLOCK;
			}
			ret = fcntl(m_fd, F_SETFL, mode);
		}
#elif WAWO_ISWIN
		// FORBIDDEN NON-BLOCKING -> SET nonBlocking to 0
		if( _nonblocking_should_set ) {
			DWORD nonBlocking = (options & OPTION_NON_BLOCKING) ? 1 : 0;
			optval = (options & OPTION_NON_BLOCKING) ? 1 : 0;
			ret = ioctlsocket(m_fd, FIONBIO, &nonBlocking) ;
		}
#else
		#error
#endif

		if(_nonblocking_should_set) {
			//WAWO_DEBUG( "[socket][#%d:%s] socket set Socket::OPTION_NON_BLOCKING, opval: %d, op ret: %d", m_fd, m_addr.AddressInfo().CStr(), optval, ret );

			if( ret == 0 ) {
				if( optval == 1 ) {
					m_option |= OPTION_NON_BLOCKING;
				} else {
					m_option &= ~OPTION_NON_BLOCKING;
				}
			} else {
				WAWO_ASSERT( ret == -1 );
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
				WAWO_FATAL( "[socket][#%d:%s] socket set Socket::OPTION_NON_BLOCKING failed, errno: %d", m_fd, m_addr.AddressInfo().CStr(), m_ec );

				return m_ec ;
			}
		}

		if ( m_sockt == ST_STREAM) {

			should_set = (((m_option&OPTION_NODELAY) && ((options&OPTION_NODELAY)==0)) ||
							(((m_option&OPTION_NODELAY)==0) && ((options&OPTION_NODELAY)))) ;

			if( should_set ) {
				optval = (options & OPTION_NODELAY) ? 1 : 0;
				ret = setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
				//WAWO_DEBUG( "[socket][#%d:%s] socket set Socket::OPTION_NODELAY, optval: %d, op ret: %d", m_fd , m_addr.AddressInfo().CStr(), optval, ret );

				if( ret == 0 ) {
					if( optval == 1 ) {
						m_option |= OPTION_NODELAY;
					} else {
						m_option &= ~OPTION_NODELAY;
					}
				} else {
					WAWO_ASSERT( ret  == -1);

					m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
					WAWO_FATAL( "[socket][#%d:%s] socket set Socket::OPTION_NODELAY failed, errno: %d", m_fd , m_addr.AddressInfo().CStr(), m_ec );

					return m_ec ;
				}
			}
		}

		return wawo::OK ;
	}

	int SocketBase::TurnOnKeepAlive() {
#if WAWO_ISGNU
		int keepalive = 1;
#elif WAWO_ISWIN
		char keepalive = 1;
#else
	#error
#endif
		return setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive) );
	}

	int SocketBase::TurnOffKeepAlive() {
#if WAWO_ISGNU
		int keepalive = 0;
#elif WAWO_ISWIN
		char keepalive = 0;
#else
#error
#endif
		return setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
	}

	int SocketBase::SetKeepAliveVals(KeepAliveVals const& vals) {
		if (vals.onoff == 0) {
			return TurnOffKeepAlive();
		}

#if WAWO_ISWIN
		DWORD dwBytesRet;
		struct tcp_keepalive alive;
		memset(&alive, 0, sizeof(alive));

		alive.onoff = 1;
		if (vals.idle == 0) {
			alive.keepalivetime = WAWO_DEFAULT_KEEPALIVE_IDLETIME;
		}
		else {
			alive.keepalivetime = vals.idle;
		}

		if (vals.interval == 0) {
			alive.keepaliveinterval = WAWO_DEFAULT_KEEPALIVE_INTERVAL;
		}
		else {
			alive.keepaliveinterval = vals.idle;
		}

		int rt = WSAIoctl(m_fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL);
		return rt;
#elif WAWO_ISGNU
		int rt = TurnOnKeepAlive();
		WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);

		if (vals.idle != 0) {
			i32_t idle = (vals.idle/1000);
			rt = setsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
			WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);
		}
		if (vals.interval != 0) {
			i32_t interval = (vals.interval/1000);
			rt = setsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
			WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);
		}
		if (vals.probes != 0) {
			rt = setsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &vals.probes, sizeof(vals.probes));
			WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);
		}
		return wawo::OK;
#else
		#error
#endif
	}

	int SocketBase::GetKeepAliveVals(KeepAliveVals& vals) {
#if WAWO_ISWIN
		WAWO_THROW_EXCEPTION("not supported");
#elif WAWO_ISGNU
		KeepAliveVals _vals;
		int rt;
		socklen_t len ;
		rt = getsockopt(m_fd, SOL_TCP, TCP_KEEPIDLE, &_vals.idle, &len);
		WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);
		rt = getsockopt(m_fd, SOL_TCP, TCP_KEEPINTVL, &_vals.interval,&len);
		WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);
		rt = getsockopt(m_fd, SOL_TCP, TCP_KEEPCNT, &_vals.probes, &len);
		WAWO_RETURN_V_IF_NOT_MATCH(SocketGetLastErrno(), rt == 0);

		_vals.idle = _vals.idle*1000;
		_vals.interval = _vals.interval*1000;
		vals = _vals;
		return wawo::OK;
#else
		#error
#endif
	}

	int SocketBase::GetTOS(u8_t& tos) const {
		u8_t _tos;
		socklen_t len;

		int rt = getsockopt(m_fd, IPPROTO_IP, IP_TOS, (char*)&_tos, &len );
		WAWO_RETURN_V_IF_NOT_MATCH( wawo::SocketGetLastErrno(), rt == 0 );
		tos = IPTOS_TOS(_tos);
		return rt;
	}

	int SocketBase::SetTOS(u8_t const& tos) {
		WAWO_ASSERT(m_fd>0);
		u8_t _tos = IPTOS_TOS(tos) | 0xe0;
		return setsockopt(m_fd, IPPROTO_IP, IP_TOS, (char*)&_tos, sizeof(_tos));
	}

	u32_t SocketBase::SendTo( wawo::byte_t const* const buff, wawo::u32_t const& byte_len, const wawo::net::SocketAddr& addr, int& ec_o ) {

		sockaddr_in addr_in;
		ec_o = wawo::OK;

		WAWO_ASSERT( addr.GetNetSequenceUlongIp() != 0 ) ;
		WAWO_ASSERT( addr.GetNetSequencePort() != 0 ) ;

		addr_in.sin_family		= AF_INET;
		addr_in.sin_addr.s_addr	= addr.GetNetSequenceUlongIp();
		addr_in.sin_port		= addr.GetNetSequencePort();

		int sent = sendto(m_fd, reinterpret_cast<const char*>(buff), byte_len, 0, reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in) );

		if( sent == -1 ) {
			m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
			ec_o = m_ec;
			WAWO_FATAL ( "[socket][#%d:%s]socket SendTo() == %d, failed", m_fd, m_addr.AddressInfo().CStr(), m_ec );
		} else {
			if( ((u32_t)sent) != byte_len ) {
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(SocketGetLastErrno());
				ec_o = m_ec;
				WAWO_FATAL("[socket][#%d:%s]Socket::SendTo() expect to send: %d, but sent: %d", m_fd, m_addr.AddressInfo().CStr(), byte_len, sent );
			}
		}

		WAWO_DEBUG("[socket][#%d:%s]Socket::SendTo(%s) == %d", m_fd, m_addr.AddressInfo().CStr(), buff, sent );
		return sent;
	}

	u32_t SocketBase::RecvFrom( byte_t* const buff_o, wawo::u32_t const& size, SocketAddr& addr_o, int& ec_o ) {
		sockaddr_in addr_in ;
		ec_o = wawo::OK;

		socklen_t slen = sizeof(addr_in);

		int received = recvfrom( m_fd, reinterpret_cast<char*>(buff_o), size, 0, reinterpret_cast<sockaddr*>(&addr_in), &slen );

		int _ern = SocketGetLastErrno();

		if( received < 0 ) {
			if( IS_ERRNO_EQUAL_WOULDBLOCK(_ern) ) {
				ec_o = _ern;
				WAWO_DEBUG("[socket][#%d:%s]recvfrom(%d) EWOULDBLOCK", m_fd, m_addr.AddressInfo().CStr() );
				received = 0;
			} else {
				m_ec = WAWO_TRANSLATE_SOCKET_ERROR_CODE(_ern) ;
				ec_o = m_ec;
				WAWO_FATAL("[socket][#%d:%s]recvfrom(%d) ERROR: %d", m_fd, m_addr.AddressInfo().CStr(), _ern );
			}
		}

		addr_o.SetNetSequencePort( ( addr_in.sin_port ) );
		addr_o.SetNetSequenceUlongIp( addr_in.sin_addr.s_addr );

		return received ;
	}

	u32_t SocketBase::Send( byte_t const* const buffer, u32_t const& size, int& ec_o, int const& block_time , int const& flags ) {
		WAWO_ASSERT( buffer != NULL );
		WAWO_ASSERT( size > 0 );

		if ( m_fd < 0 ) {
			ec_o = wawo::E_SOCKET_NOT_CONNECTED;
			return 0;
		}

		if( m_shutdown_flag&SHUTDOWN_WR ) {
			ec_o = wawo::E_SOCKET_WR_SHUTDOWN_ALREADY ;
			return 0;
		}

		u32_t tt = 0 ; //try time
		u64_t begin_time = 0 ;
		u32_t sent_total = 0;

		//TRY SEND
		do {
			int sent = send( m_fd, reinterpret_cast<const char*>(buffer) + sent_total, size-sent_total, flags );
			if( sent>0 ) {

				ec_o = wawo::OK ;
				sent_total += sent;
				if( sent_total == size ) {
					break;
				}

				tt = 0;
				begin_time = 0;
			} else {
				//error
				if( sent == -1 ) {
					int send_ec = SocketGetLastErrno();
					if( send_ec == EINTR ) {
						continue;
					} else if( IS_ERRNO_EQUAL_WOULDBLOCK(send_ec) ) {
						WAWO_ASSERT( IsNonBlocking() ); //is nonblocking, can blocking io get this error ?

						if( block_time == 0 ) {
							ec_o = wawo::E_SOCKET_SEND_IO_BLOCK;
							WAWO_FATAL("[socket][#%d:%s] socketbase::send blocked, error code: <%d>, do not try with 0 blocktime" , m_fd, m_addr.AddressInfo().CStr(), send_ec ) ;
							break;
						}

						WAWO_FATAL("[socket][#%d:%s] socketbase::send blocked, error code: <%d>, try_time: %d" , m_fd, m_addr.AddressInfo().CStr(), send_ec, tt ) ;
						u64_t now = wawo::time::curr_milliseconds();
						if( begin_time == 0 ) {
							begin_time = now;
						}

						if( (now-begin_time)>((u32_t)block_time) ) {
							ec_o = wawo::E_SOCKET_SEND_IO_BLOCK;
							WAWO_FATAL("[socket][#%d:%s] socketbase::send blocked, error code: <%d>, try failed, try_time: %d in %dms, max blocktime:%d" , m_fd, m_addr.AddressInfo().CStr(), send_ec, tt, (now-begin_time), block_time) ;
							break;
						} else {
							++tt;
							wawo::usleep(WAWO_MIN(__WAIT_FACTOR__*tt, __FLUSH_SLEEP_TIME_MAX__));
						}
					} else {
						WAWO_FATAL("[socket][#%d:%s] socketbase::send failed, error code: <%d>" , m_fd, m_addr.AddressInfo().CStr(), send_ec ) ;
						ec_o = WAWO_TRANSLATE_SOCKET_ERROR_CODE(send_ec) ;
						break;
					}
				}
			}
		} while( sent_total < size );

		WAWO_DEBUG("[socket][#%d:%s] socketbase::send, to send: %d, sent: %d, ec: %d" , m_fd, m_addr.AddressInfo().CStr(), size, sent_total, ec_o ) ;
		return sent_total ;
	}

	u32_t SocketBase::Recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag ) {
		WAWO_ASSERT( buffer_o != NULL );
		WAWO_ASSERT( size > 0 );

		if( m_fd<0 ) {
			ec_o = wawo::E_SOCKET_NOT_CONNECTED ;
			return 0;
		}

		if( m_shutdown_flag&SHUTDOWN_RD ) {
			ec_o = wawo::E_SOCKET_RD_SHUTDOWN_ALREADY ;
			return 0;
		}

		u32_t r_total = 0;
		do {
			int r = recv( m_fd, reinterpret_cast<char*>( buffer_o) + r_total, size - r_total, flag );
			if( r > 0 ) {
				r_total += r ;
				ec_o = wawo::OK ;
				break;
			} else if ( r == 0 ) {
				WAWO_DEBUG("[socket][#%d:%s] socket has been gracefully closed by remote side[detected by recv]", m_fd, m_addr.AddressInfo().CStr() );
				ec_o = wawo::E_SOCKET_REMOTE_GRACE_CLOSE;
				break;
			} else if( r == -1 ) {
				int ec = SocketGetLastErrno();
				if( IS_ERRNO_EQUAL_WOULDBLOCK(ec) ) {
					WAWO_ASSERT( IsNonBlocking() );
					WAWO_DEBUG("[socket][#%d:%s] socketbase::recv blocked, block code: <%d>" , m_fd, m_addr.AddressInfo().CStr(), ec ) ;
					ec_o = wawo::E_SOCKET_RECV_IO_BLOCK ;
					break;
				} else if( ec == EINTR ) {
					continue;
				} else {
					ec_o = WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec) ;
					WAWO_FATAL("[socket][#%d:%s] socketbase::recv error, errno: %d", m_fd, m_addr.AddressInfo().CStr(), ec );
					break;
				}
			} else {
				WAWO_THROW_EXCEPTION( "[socket] socketbase::recv; are u kiddng me !!!!" );
			}
		} while(true) ;

		WAWO_DEBUG("[socket][#%d:%s] socketbase::recv bytes, %d", m_fd, m_addr.AddressInfo().CStr(), r_total);
		return r_total ;
	}

	int SocketBase::TLP_Handshake(bool const& nonblocking ) {
		WAWO_ASSERT(!"TOIMPL");
		return wawo::OK;
	}


	void Socket::_Init() {
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
	}

	void Socket::_Deinit() {
		free( m_tsb );
		m_tsb = NULL;

		free( m_trb );
		m_trb = NULL;

		WAWO_ASSERT( m_rb != NULL );
		WAWO_DELETE( m_rb );

		WAWO_ASSERT( m_sb != NULL );
		WAWO_DELETE( m_sb );
	}

	int Socket::Close( int const& ec ) {

		if( IsNonBlocking() &&
			IsDataSocket() &&
			!IsWriteShutdowned() )
		{
			u32_t left;
			int ec;
			Flush(left,ec, __FLUSH_BLOCK_TIME_INFINITE__);
		}

		LockGuard<SpinMutex> lg( m_mutexes[L_SOCKET] );
		if (m_state == S_CLOSED) { return wawo::E_SOCKET_INVALID_STATE; }

		int old_flag = m_shutdown_flag;
		int old_state = m_state;
		int closert = _Close( ec );
		int new_flag = m_shutdown_flag;

		if (!IsNonBlocking()) return closert;

		WAWO_ASSERT( m_fd < 0 );
		WAWO_ASSERT( IsListenSocket() ? ((m_state == S_CLOSED)&&(m_shutdown_flag == SHUTDOWN_NONE)): 1 );
		WAWO_ASSERT( IsDataSocket() ? ((m_state == S_CLOSED)&&(m_shutdown_flag == SHUTDOWN_RDWR)) : 1 );

		if (IsListenSocket()) {
			WWRP<SocketEvent> evt_close(new SocketEvent(WWRP<Socket>(this), SE_CLOSE, Cookie(ec)));
			_DispatcherT::OSchedule(evt_close);
		} else {

			if (old_state != S_CONNECTED) {
				WWRP<SocketEvent> evt_error(new SocketEvent(WWRP<Socket>(this), SE_ERROR, Cookie(ec)));
				_DispatcherT::OSchedule(evt_error);
				return closert;
			}

			WWRP<SocketEvent> evt_rd;
			WWRP<SocketEvent> evt_wr;
			WWRP<SocketEvent> evt_close(new SocketEvent(WWRP<Socket>(this), SE_CLOSE, Cookie(ec)));

			if (old_flag == new_flag) {
				_DispatcherT::OSchedule(evt_close);
				return closert;
			}

			WWRP<_DispatcherT> dp(this);
			__CheckShutdownEvent(old_flag^new_flag, ec, evt_rd, evt_wr);
			WAWO_ASSERT(evt_rd != NULL || evt_wr != NULL);
			_DispatcherT::LambdaFnT lambda_FNR = [dp, evt_rd, evt_wr, evt_close]() -> void {
				if (evt_rd != NULL) {
					dp->Trigger(evt_rd);
				}
				if (evt_wr != NULL) {
					dp->Trigger(evt_wr);
				}
				dp->Trigger(evt_close);
			};
			_DispatcherT::OSchedule(lambda_FNR, nullptr);
		}

		return closert;
	}

	int Socket::Shutdown(int const& flag, int const& ec ) {

		if( IsNonBlocking() &&
			IsDataSocket() &&
			!IsWriteShutdowned() &&
			flag&SHUTDOWN_WR )
		{
			u32_t left;
			int ec;
			Flush(left,ec);
		}

		LockGuard<SpinMutex> lg( m_mutexes[L_SOCKET] );
		if( (m_shutdown_flag&flag) == flag ) {
			return wawo::E_SOCKET_INVALID_OPERATION;
		}

		int old_flag = m_shutdown_flag;
		int shutrt = _Shutdown( flag, ec );
		int new_flag = m_shutdown_flag;
		if( old_flag != new_flag) {
			WWRP<SocketEvent> evt_rd;
			WWRP<SocketEvent> evt_wr;
			__CheckShutdownEvent( old_flag^new_flag, ec,evt_rd,evt_wr );
			WAWO_ASSERT( evt_rd != NULL || evt_wr != NULL );
			WWRP<_DispatcherT> dp(this);
			_DispatcherT::LambdaFnT lambda_FNR = [dp, evt_rd, evt_wr]() -> void {
				if (evt_rd != NULL) {
					dp->Trigger(evt_rd);
				}
				if (evt_wr != NULL) {
					dp->Trigger(evt_wr);
				}
			};
			_DispatcherT::OSchedule(lambda_FNR,nullptr);
		}
		return shutrt;
	}

	u32_t Socket::Accept( WWRP<Socket> sockets[], u32_t const& size, int& ec_o ) {

		WAWO_ASSERT( m_sm == SM_LISTENER );

		if( m_state != S_LISTEN ) {
			ec_o = wawo::E_SOCKET_INVALID_STATE;
			return 0 ;
		}

		ec_o = wawo::OK;
		u32_t count = 0;
		sockaddr_in addr_in;
		socklen_t addr_len = sizeof(addr_in);

		LockGuard<SpinMutex> lg( m_mutexes[L_READ] );
		WAWO_ASSERT( m_rs == S_READ_POSTED || m_rs == S_READ_CONTINUE);
		m_rs = S_READING;

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
			addr.SetNetSequencePort( (addr_in.sin_port) );
			addr.SetNetSequenceUlongIp( (addr_in.sin_addr.s_addr) );

			WWRP<Socket> socket( new Socket(fd, addr, SM_PASSIVE, m_sbc, m_family, m_sockt, m_protocol, OPTION_NONE) );
			sockets[count++] = socket;
		} while( count<size );

		if( count == size ) {
			ec_o=wawo::E_TRY_AGAIN;
		}

		m_rs = (ec_o == wawo::E_TRY_AGAIN) ? S_READ_CONTINUE : S_READ_IDLE;
		return count ;
	}

	u32_t Socket::Send( byte_t const* const buffer, u32_t const& size, int& ec_o, int const& block_time, int const& flags ) {

		WAWO_ASSERT( buffer != NULL );
		WAWO_ASSERT( size > 0 );
		WAWO_ASSERT( size <= m_rb->Capacity() );

		if( m_fd < 0 ) {
			ec_o = wawo::E_SOCKET_NOT_CONNECTED;
			return 0;
		}

		LockGuard<SpinMutex> lg( m_mutexes[L_WRITE]);
		if( IsNonBlocking() ) {
			WAWO_ASSERT( m_sb != NULL );
			u32_t wcount = 0;
			if( m_sb->BytesCount() > 0 ) {
				if (flags&F_SND_USE_ASYNC_BUFFER) {
					if (m_sb->LeftCapacity() > size) {
						wcount = m_sb->Write((byte_t*)(buffer), size);
						WAWO_ASSERT(wcount == size);
						WAWO_DEBUG("[socket][#%d:%s] Socket::Send(): ringbuffer not empty, ringbuffer write: %d", m_fd, m_addr.AddressInfo().CStr(), wcount);
					} else {
						ec_o = wawo::E_SOCKET_SEND_BUFFER_NOT_ENOUGH;
						WAWO_FATAL("[socket][#%d:%s] Socket::Send(): ringbuffer not empty, ringbuffer left space: %d, to write: %d, please retry again later", m_fd, m_addr.AddressInfo().CStr(), m_sb->LeftCapacity(), size);
					}
					return wcount;
				}
				ec_o = wawo::E_SOCKET_SEND_IO_BLOCK;
				return 0;
			}
		}

		u32_t sent = SocketBase::Send(buffer,size,ec_o,block_time);
		if( sent<size ) {

			if( ec_o == wawo::E_SOCKET_SEND_IO_BLOCK ) {
				WAWO_ASSERT( IsNonBlocking() );
				WAWO_ASSERT( m_sb != NULL );

				//write to ring buffer for send, and register this socket to CHECK_WRITE observer
				//calc the rest, and push it into send buffer
				u32_t to_buffer_c = size-sent ;

				if( m_sb->LeftCapacity() < to_buffer_c ) {
					WAWO_WARN("[socket][#%d:%s] Socket::Send() blocked (ringbuffer not enough), socket::sent: alread sent %d," , m_fd, m_addr.AddressInfo().CStr(), sent ) ;
					ec_o = wawo::E_SOCKET_SEND_BUFFER_NOT_ENOUGH ;
				} else {

					wawo::u32_t write_c = m_sb->Write( (byte_t*)(buffer) + sent, to_buffer_c ) ;
					WAWO_ASSERT( to_buffer_c == write_c );

					WAWO_DEBUG("[socket][#%d:%s] Socket::Send() blocked (write to ringbuffer: %d), socket::sent: %d" , m_fd, m_addr.AddressInfo().CStr(), write_c, sent ) ;
					sent += write_c;

					WWRP<SocketEvent> evt( new SocketEvent(WWRP<Socket>(this), SE_SEND_BLOCKED ));
					_DispatcherT::Schedule( evt );
				}
			}
		}

		return sent;
	}

	u32_t Socket::Recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag ) {
		LockGuard<SpinMutex> lg( m_mutexes[L_READ]);
		return SocketBase::Recv( buffer_o, size, ec_o, flag );
	}

	u32_t Socket::_Pump(int& ec_o, int const& flag ) {

		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( m_rb != NULL );
		ec_o = wawo::OK;

		u32_t left_space = m_rb->LeftCapacity() ;
		if( 0 == left_space ) {
			WAWO_FATAL("[socket][#%d:%s] pump m_rb->LeftSpace() == 0, receiving buffer full, ZERO bytes read, COULD TRY", m_fd, m_addr.AddressInfo().CStr());
			ec_o = wawo::E_SOCKET_PUMP_TRY_AGAIN ;
			return 0;
		}

		u32_t can_trb_recv_s = ( m_sbc.rcv_size <= left_space) ?  m_sbc.rcv_size : left_space ;
		u32_t recv_BC = SocketBase::Recv( m_trb, can_trb_recv_s, ec_o,flag );
		if (recv_BC > 0) {
			wawo::u32_t rb_WBC = m_rb->Write(m_trb, recv_BC);
			WAWO_ASSERT(recv_BC == rb_WBC);
			(void)rb_WBC;
		}

		WAWO_DEBUG("[socket][#%d:%s]pump bytes: %d, ec: %d", m_fd, m_addr.AddressInfo().CStr() , recv_BC, ec_o );
		return recv_BC;
	}

	void Socket::HandleAsyncPump() {

		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( m_rb != NULL );
		WAWO_ASSERT( IsNonBlocking() );

		LockGuard<SpinMutex> lg( m_mutexes[L_READ]);
		WAWO_ASSERT( m_rs == S_READ_POSTED );
		m_rs = S_READING;

		int recv_ec;
		int recv_flag = F_RCV_ALWAYS_PUMP;
		do {
			WWSP<Packet> arrives[5];
			u32_t count = _ReceivePackets(arrives, 5, recv_ec, recv_flag);
			for (u32_t i = 0; i < count; i++) {
				WWRP<SocketEvent> evt(new SocketEvent(WWRP<Socket>(this), SE_PACKET_ARRIVE, arrives[i]) );
				_DispatcherT::Trigger(evt);
			}
			recv_flag = (recv_ec == wawo::E_SOCKET_PUMP_TRY_AGAIN) ? F_RCV_ALWAYS_PUMP : 0;
		} while(recv_ec == wawo::E_TLP_TRY_AGAIN || recv_ec == wawo::E_SOCKET_PUMP_TRY_AGAIN );

		switch(recv_ec) {
		case wawo::E_SOCKET_RECV_IO_BLOCK:
		case wawo::OK:
			{
			}
			break;
		case wawo::E_SOCKET_REMOTE_GRACE_CLOSE:
			{
				Shutdown(SHUTDOWN_RD, recv_ec);
			}
			break;
		case wawo::E_SOCKET_NOT_CONNECTED:
		case wawo::E_SOCKET_RD_SHUTDOWN_ALREADY:
			{
				WAWO_DEBUG("[socket][#%d:%s]async pump error, ec: %d", m_fd, m_addr.AddressInfo().CStr() , recv_ec);
				WWRP<SocketEvent> sevt( new SocketEvent( WWRP<Socket>(this), SE_ERROR, Cookie( wawo::E_SOCKET_RD_SHUTDOWN_ALREADY )));
				_DispatcherT::OSchedule(sevt);
			}
			break;
		case wawo::E_SOCKET_RECV_BUFFER_FULL:
		default:
			{
				WAWO_DEBUG("[socket][#%d:%s] socket pump error: %d, shutdown", m_fd, m_addr.AddressInfo().CStr(), recv_ec);
				Shutdown(SHUTDOWN_RDWR, recv_ec);
			}
			break;
		}

		m_rs = S_READ_IDLE ;
	}

	u32_t Socket::Flush(u32_t& left, int& ec_o, int const& block_time /* in milliseconds*/) {

		if( !IsNonBlocking() ) { ec_o = wawo::E_INVALID_OPERATION; return 0 ; }

		WAWO_ASSERT( m_sb != NULL );
		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u64_t begin_time = 0 ;
		u32_t k = 0;

		LockGuard<SpinMutex> lg( m_mutexes[L_WRITE] );
		do {
			flushed_total += __Flush( left, ec_o );
			if( (left == 0) || (ec_o != wawo::E_SOCKET_SEND_IO_BLOCK) || block_time == 0 ) {
				break;
			}

			u64_t now = wawo::time::curr_milliseconds();
			if(begin_time==0) {
				begin_time = now;
			}

			if( (block_time != -1) && (begin_time-now)>((u32_t)block_time) ) {
				break;
			}
			wawo::usleep(WAWO_MIN(__WAIT_FACTOR__*k++, __FLUSH_SLEEP_TIME_MAX__) );
		} while( true );

		WAWO_DEBUG( "[socket][#%d:%s]Flush end, sent: %d, left: %d", m_fd, m_addr.AddressInfo().CStr(), flushed_total, left );
		return flushed_total;
	}

	u32_t Socket::HandleAsyncFlush(u32_t& left, int& ec_o, int const& block_time /* in milliseconds*/) {

		WAWO_ASSERT( IsNonBlocking() );
		WAWO_ASSERT( m_sb != NULL );
		ec_o = wawo::OK ;

		u32_t flushed_total = 0;
		u64_t begin_time = 0 ;
		u32_t k = 0;

		LockGuard<SpinMutex> lg( m_mutexes[L_WRITE] );
		WAWO_ASSERT( m_ws == S_WRITE_POSTED );
		m_ws = S_WRITING;
		do {
			flushed_total += __Flush( left, ec_o );
			if( (left == 0) || (ec_o != wawo::E_SOCKET_SEND_IO_BLOCK) || block_time==0 ) {
				break;
			}

			u64_t now = wawo::time::curr_milliseconds();
			if(begin_time==0) {
				begin_time = now;
			}
			if( (block_time!=-1)&&(begin_time-now)>(( u32_t)block_time) ) {
				break;
			}
			wawo::usleep(WAWO_MIN(__WAIT_FACTOR__*k++, __FLUSH_SLEEP_TIME_MAX__));
		} while( true );

		if( left == 0 ) {
			WAWO_ASSERT( ec_o == wawo::OK );
			m_async_wt = 0;
		} else {
			if( ec_o == wawo::E_SOCKET_SEND_IO_BLOCK ) {
				u64_t current = wawo::time::curr_milliseconds() ;

				if( (flushed_total == 0) && (m_async_wt != 0) && ((current - m_async_wt) > m_delay_wp ) ) {
					ec_o = wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED;
				} else {
					if( (m_async_wt==0) || (flushed_total>0) ) {
						m_async_wt = current; //update timer
					}
				}
			}
		}
		m_ws = S_WRITE_IDLE;

		WAWO_DEBUG( "[socket][#%d:%s]AsyncFlush end, sent: %d, left: %d", m_fd, m_addr.AddressInfo().CStr(), flushed_total, left );
		return flushed_total;
	}

	u32_t Socket::__Flush( u32_t& left, int& ec_o ) {
		WAWO_ASSERT( IsDataSocket() );
		WAWO_ASSERT( IsNonBlocking() );

		u32_t flushed = 0;
		left = m_sb->BytesCount();
		ec_o = wawo::OK;

		while( (left!=0) && (ec_o == wawo::OK) ) {
			u32_t to_send = m_sb->Peek( m_tsb, m_sbc.snd_size );
			u32_t sent = SocketBase::Send( m_tsb, to_send, ec_o, 0) ;
			flushed += sent ;

			if( sent != to_send ) {
				WAWO_WARN( "[socket][#%d:%s]Socket::__Flush, try to __flush: %d, but only sent: %d, ec: %d", m_fd, m_addr.AddressInfo().CStr(), to_send, sent, ec_o );
			}
			if( sent > 0 ) {
				m_sb->Skip(sent);
			}
			left = m_sb->BytesCount() ;
		}

		WAWO_DEBUG("[socket][#%d:%s]__Flush, flushed bytes: %d, left: %d, ec: %d", m_fd, m_addr.AddressInfo().CStr() , flushed, left, ec_o );
		return flushed;
	}

	int Socket::SendPacket(WWSP<Packet> const& packet, int const& flag ) {
		WAWO_ASSERT( packet != NULL);
		WAWO_ASSERT( m_tlp != NULL );
		WWSP<Packet> TLP_Pack;

		int rt = m_tlp->Encode(packet, TLP_Pack);
		if (rt != wawo::OK) {
			return rt;
		}

		int send_ec;
		return (Socket::Send(TLP_Pack->Begin(), TLP_Pack->Length(), send_ec, __SEND_BLOCK_TIME__, flag) == TLP_Pack->Length()) ? wawo::OK : send_ec;
	}

	u32_t Socket::ReceivePackets(WWSP<Packet> arrives[], u32_t const& size, int& ec_o, int const& flag ) {
		LockGuard<SpinMutex> lg(m_mutexes[L_READ]);
		return _ReceivePackets(arrives,size,ec_o, flag);
	}

	//return one packet at least
	u32_t Socket::_ReceivePackets(WWSP<Packet> arrives[], u32_t const& size, int& ec_o, int const& flag ) {
		WAWO_ASSERT(m_tlp != NULL);
		bool force = (m_rb->BytesCount() ==0) ;

	force_pump:
		int pump_ec = wawo::OK ;
		bool pump_should_try = false;
		if (force || (flag&F_RCV_ALWAYS_PUMP)) {
			u32_t pumped = _Pump(pump_ec, (flag&(~(F_RCV_BLOCK_UNTIL_PACKET_ARRIVE|F_RCV_ALWAYS_PUMP))));
			if (pump_ec != wawo::OK && pump_ec != wawo::E_SOCKET_RECV_IO_BLOCK) {
				ec_o = pump_ec;
				return 0;
			}
			(void)&pumped;

			if (m_rb->BytesCount() == 0) {
				WAWO_ASSERT(IsNonBlocking());
				ec_o = pump_ec;
				return 0;
			}
			pump_should_try = ((pump_ec == wawo::OK) && IsNonBlocking());
		}

		WWSP<Packet> out;
		int tlp_ec ;
		u32_t pcount = m_tlp->DecodePackets(m_rb, arrives, size, tlp_ec, out);
		WAWO_DEBUG("[socket] new packet arrived: %d", pcount);

		if (out != NULL && out->Length()) {
			int sndec;
			u32_t snd_bytes = Send(out->Begin(), out->Length(), sndec);
			(void)&snd_bytes;
			WAWO_ASSERT(sndec == wawo::OK ? snd_bytes == out->Length() : true);
			ec_o = (sndec != wawo::OK) ? sndec : ec_o;
		}
		if ((flag&F_RCV_BLOCK_UNTIL_PACKET_ARRIVE)&&((pcount==0)&&(size>0)) && (tlp_ec == wawo::OK)) {
			force = true;
			goto force_pump;
		}
		if ((tlp_ec == wawo::E_TLP_HANDSHAKE_DONE) && IsNonBlocking()) {
			WWRP<SocketEvent> evt(new SocketEvent(WWRP<Socket>(this), SE_TLP_HANDSHAKE_DONE));
			_DispatcherT::OSchedule(evt);
			tlp_ec = wawo::OK;
		}

		if (tlp_ec != wawo::OK) {
			ec_o = tlp_ec;
			return pcount;
		}

		if (pump_should_try) {
			ec_o = wawo::E_SOCKET_PUMP_TRY_AGAIN;
			return pcount;
		}

		//setup may try
		if ((pcount == size) && (m_rb->BytesCount() > 0)) {
			ec_o = wawo::E_TLP_TRY_AGAIN;
			return pcount;
		}

		ec_o = tlp_ec;
		return pcount;
	}

	int Socket::TLP_Handshake(bool const& nonblocking ) {
		WAWO_ASSERT(m_tlp != NULL);
		WWSP<Packet> hello;

		int ec;
		int handshake_state = m_tlp->Handshake_MakeHelloPacket(hello);
		if (handshake_state == wawo::net::core::TLP_HANDSHAKE_DONE) {
			return wawo::OK;
		}

		int sndrt = SocketBase::Send(hello->Begin(), hello->Length(), ec);
		(void)&sndrt;
		WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);

		if (nonblocking == true) {
			return handshake_state;
		}

		while (m_tlp->Handshake_IsHandshaking()) {
			WWSP<Packet> packet_handshake;
			WWSP<Packet> arrives[1];
			u32_t count = ReceivePackets(arrives, 0, handshake_state);
			WAWO_ASSERT(count == 0);

			if (handshake_state != wawo::E_TLP_TRY_AGAIN && handshake_state != wawo::E_SOCKET_PUMP_TRY_AGAIN && handshake_state != wawo::OK && handshake_state != wawo::E_SOCKET_RECV_IO_BLOCK) {
				break;
			}

			if (IsNonBlocking()) {
				wawo::sleep(5);
			}
			(void)count;
		}

		if (m_tlp->Handshake_IsDone()) {
			return wawo::OK;
		}

		WAWO_ASSERT(handshake_state != wawo::OK);

		return handshake_state;
	}

#ifdef WAWO_PLATFORM_WIN
	class WinsockInit : public wawo::Singleton<WinsockInit> {
	public:
		WinsockInit() {
			WSADATA wsaData;
			int result = WSAStartup( MAKEWORD(2,2), &wsaData );
			WAWO_CONDITION_CHECK( result == 0 );
		}
		~WinsockInit() {
			WSACleanup();
		}
	};
	static WinsockInit const& __wawo_winsock_init__ = *(WinsockInit::GetInstance()) ;
#endif

}}
