#ifndef _WAWO_NET_SOCKET_HPP_
#define _WAWO_NET_SOCKET_HPP_

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/time/time.hpp>
#include <wawo/thread/Mutex.hpp>
#include <wawo/net/SocketAddr.hpp>
#include <wawo/algorithm/BytesRingBuffer.hpp>
#include <wawo/algorithm/Packet.hpp>

#include <wawo/net/core/Dispatcher_Abstract.hpp>
#include <wawo/net/core/TLP_Abstract.hpp>
#include <wawo/net/NetEvent.hpp>

#define WAWO_TRANSLATE_SOCKET_ERROR_CODE(_code) WAWO_NEGATIVE(_code)

namespace wawo { namespace net {
	//remark: WSAGetLastError() == GetLastError(), but there is no gurantee for future change.
	inline int SocketGetLastErrno() {
#ifdef WAWO_PLATFORM_GNU
		return errno;
#elif defined(WAWO_PLATFORM_WIN)
		return ::WSAGetLastError();
#else
		#error
#endif
	}
}}

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

//IN milliseconds
#define WAWO_MAX_ASYNC_WRITE_PERIOD			(60*1000)

//in milliseconds
#define __SEND_BLOCK_TIME__				(0)			//2 milliseconds
#define __FLUSH_BLOCK_TIME__			(500)		//500 mlliseconds
#define __FLUSH_BLOCK_TIME_INFINITE__	(-1)		//
#define __FLUSH_SLEEP_TIME_MAX__		(16*1000)	//16 ms
#define __ASYNC_FLUSH_BLOCK_TIME		(0)			//0 millisecond
#define __WAIT_FACTOR__					(100)		//50 ns

//for bad network condition ,please increae this value, in milliseconds

#ifndef _DEBUG
	#undef WAWO_CHECK_SOCKET_SEND_RETURN_V
	#define WAWO_CHECK_SOCKET_SEND_RETURN_V(V)
#endif

#define WAWO_MAX_ACCEPTS_ONE_TIME 128

namespace wawo { namespace net { namespace core {
#ifdef WAWO_PLATFORM_GNU
	typedef socklen_t socklen_t;
#elif defined(WAWO_PLATFORM_WIN)
	typedef int socklen_t ;
#else
	#error
#endif
}}}

namespace wawo { namespace net {

	using namespace wawo::thread;

	enum SockBufferConfigType {
		SBCT_SUPER_LARGE,
		SBCT_LARGE,
		SBCT_MEDIUM_UPPER,
		SBCT_MEDIUM,
		SBCT_TINY,
		SBCT_SUPER_TINY,
		SBCT_MAX
	};
	struct SockBufferConfig {
		u32_t rcv_size;
		u32_t snd_size;
	};

	SockBufferConfig GetBufferConfig( u32_t const& type ) ;

	enum SockBufferRange {
		SOCK_SND_MAX_SIZE = 1024*1024*2,
		SOCK_SND_MIN_SIZE = 1024*4,
		SOCK_RCV_MAX_SIZE = 1024*1024*2,
		SOCK_RCV_MIN_SIZE = 1024*4
	};

	enum SocketIOState {
		//read scope
		S_READ_IDLE,
		S_READ_POSTED,
		S_READING,
		S_READ_CONTINUE,

		//write scope
		S_WRITE_IDLE,
		S_WRITE_POSTED,
		S_WRITING
	};

	enum SocketFds {
		FD_NONECONN = -1,
		FD_CLOSED = -2
	};

	enum SocketMode {
		SM_NONE,
		SM_ACTIVE,
		SM_PASSIVE,
		SM_LISTENER
	};
	enum Option {
		OPTION_NONE = 0,
		OPTION_BROADCAST = 0x01, //only for UDP
		OPTION_REUSEADDR = 0x02,
		OPTION_NON_BLOCKING = 0x04,
		OPTION_NODELAY = 0x08, //only for TCP
	};

	enum ShutdownFlag {
		SHUTDOWN_NONE = 0,
		SHUTDOWN_RD = 0x01,
		SHUTDOWN_WR = 0x02,
		SHUTDOWN_RDWR = (SHUTDOWN_RD | SHUTDOWN_WR)
	};

	enum State {
		S_IDLE = 0,
		S_OPENED,
		S_BINDED,
		S_LISTEN,
		S_CONNECTING,// for async connect
		S_CONNECTED,
		S_CLOSED
	};

	enum LockType {
		L_SOCKET = 0,
		L_READ,
		L_WRITE,
		L_MAX
	};

	struct KeepAliveVals {
		u8_t	onoff;
		i32_t 	idle; //in milliseconds
		i32_t	interval; //in milliseconds
		i32_t	probes;

		KeepAliveVals() :
			onoff(0),
			idle(0),
			interval(0),
			probes(0)
		{}
	};
	class SocketBase:
		virtual public wawo::RefObject_Abstract
	{

	public:

		explicit SocketBase( Family const& family, SockT const& sockt, Protocol const& protocol,Option const& option = OPTION_NONE );
		explicit SocketBase( SockBufferConfig const& sbc ,Family const& family, SockT const& sockt, Protocol const& proto,Option const& option = OPTION_NONE ); //init a empty socket object
		explicit SocketBase( SocketAddr const& addr, Family const& family, SockT const& sockt, Protocol const& proto, Option const& option = OPTION_NONE ); //init a empty socket object
		explicit SocketBase( SocketAddr const& addr, SockBufferConfig const& sbc,Family const& family, SockT const& sockt, Protocol const& proto, Option const& option = OPTION_NONE ); //init a empty socket object
		explicit SocketBase( int const& fd, SocketAddr const& addr, SocketMode const& sm , SockBufferConfig const& sbc ,Family const& family , SockT const& sockt, Protocol const& proto , Option const& option = OPTION_NONE ); //by pass a connected socket fd

		virtual ~SocketBase();

		inline SocketMode const& GetMode() const { return m_sm; }

		inline bool IsPassive() const { return m_sm == SM_PASSIVE; }
		inline bool IsActive() const {return m_sm == SM_ACTIVE;}
		inline bool IsListener() const {return m_sm ==SM_LISTENER;}

		inline bool IsDataSocket() const { return GetMode() != SM_LISTENER ;}
		inline bool IsListenSocket() const { return GetMode() == SM_LISTENER; }

		inline int const& GetFd() const { return m_fd ;}
		SocketAddr const& GetRemoteAddr() const { return m_addr; }
		SocketAddr GetLocalAddr() const;

		int Open();
		int Shutdown(int const& flag, int const& ec = wawo::OK);
		int Close( int const& ec = wawo::OK);

		inline bool IsIdle() const {return m_state == S_IDLE ;}
		inline bool IsOpened() const {return m_state == S_OPENED ;}
		inline bool IsBinded() const {return m_state == S_BINDED; }
		inline bool IsConnected() const {return m_state == S_CONNECTED; }
		inline bool IsReadShutdowned() const {return (IsClosed()) || (m_shutdown_flag&SHUTDOWN_RD) ;}
		inline bool IsWriteShutdowned() const {return (IsClosed()) || (m_shutdown_flag&SHUTDOWN_WR) ;}
		inline bool IsReadWriteShutdowned() const {return (IsClosed()) || m_shutdown_flag==SHUTDOWN_RDWR; }
		inline bool IsClosed() const { return (m_state == S_CLOSED); }

		inline State const& GetState() const { return m_state;}
		inline bool IsConnecting() const { return m_is_connecting; }

		int Bind( SocketAddr const& addr );
		int Listen( int const& backlog );
		inline bool IsListening() const { return (m_fd>0) && (m_state == S_LISTEN); }

		//return socketFd if success, otherwise return -1 if an error is set
		u32_t Accept( WWRP<SocketBase> sockets[], u32_t const& size, int& ec_o );
		int Connect();
		int AsyncConnect();

		void HandleAsyncConnected( int& ec_o );

		u32_t Send( byte_t const* const buffer, u32_t const& size, int& ec_o,int const& block_time = __SEND_BLOCK_TIME__, int const& flags = 0 );
		u32_t Recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0 );

		u32_t SendTo( byte_t const* const buff, wawo::u32_t const& size, const SocketAddr& addr, int& ec_o );
		u32_t RecvFrom( byte_t* const buff_o, wawo::u32_t const& size, SocketAddr& addr, int& ec_o );

		inline int GetErrorCode() const { return m_ec; };

		inline bool IsNoDelay() const { return ((m_option&OPTION_NODELAY) != 0) ; }

		int TurnOffNoDelay();
		int TurnOnNoDelay();

		int TurnOnNonBlocking();
		int TurnOffNonBlocking();
		inline bool IsNonBlocking() const {return ( (m_option&OPTION_NON_BLOCKING) != 0) ;}

		//must be called between open and connect|listen
		int SetSndBufferSize( u32_t const& size );
		int GetSndBufferSize( u32_t& size ) const;
		int GetLeftSndQueue( u32_t& size) const;


		//must be called between open and connect|listen
		int SetRcvBufferSize( u32_t const& size );
		int GetRcvBufferSize( u32_t& size ) const;
		int GetLeftRcvQueue( u32_t& size) const;

		int GetLinger( bool& on_off, int& linger_t ) const ;
		int SetLinger( bool const& on_off, int const& linger_t = 30 /* in seconds */ ) ;

		int TurnOnKeepAlive();
		int TurnOffKeepAlive();

		/*	please note that
			windows do not provide ways to retrieve idle time and interval ,and probes has been disabled programming since vista
		*/

		int SetKeepAliveVals(KeepAliveVals const& vals);
		int GetKeepAliveVals(KeepAliveVals& vals);

		inline SpinMutex& GetLock( LockType const& lt) {  WAWO_ASSERT( lt < L_MAX ); return m_mutexes[lt]; }

		inline SocketIOState const& GetRState() const { return m_rs;}
		inline SocketIOState const& GetWState() const { return m_ws;}
		inline void SetRState( SocketIOState const& state ) { m_rs = state;}
		inline void SetWState( SocketIOState const& state ) { m_ws = state;}

		inline WWRP<core::TLP_Abstract> const& GetTLP() {
			LockGuard<SpinMutex> _lg(m_mutexes[L_SOCKET]);
			return m_tlp;
		}

		inline void SetTLP(WWRP<core::TLP_Abstract> const& tlp) {
			LockGuard<SpinMutex> _lg(m_mutexes[L_SOCKET]);
			WAWO_ASSERT(m_tlp == NULL);
			WAWO_ASSERT(tlp != NULL);
			m_tlp = tlp;
		}

		virtual int TLP_Handshake(bool const& nonblocking = true);

	protected:
		int _Close( int const& ec = wawo::OK);
		int _Shutdown(int const& flag, int const& ec );
		int _Connect();

		int SetOptions( int const& options );

		SocketMode m_sm; //
		Family m_family;
		SockT m_sockt;
		Protocol m_protocol;

		int m_option;

		SocketAddr m_addr ;

		int m_fd;
		int m_ec;

		State		m_state;
		u8_t		m_shutdown_flag;

		SpinMutex m_mutexes[L_MAX];

		SockBufferConfig m_sbc; //socket buffer setting

		bool m_is_connecting; //lock by w-mutex

		SocketIOState m_rs; //m_read state
		SocketIOState m_ws; //m_write state

		WWRP<core::TLP_Abstract>	m_tlp;
	};

	enum SocketFlag {
		F_RCV_ALWAYS_PUMP				= 0x1000000,
		F_RCV_BLOCK_UNTIL_PACKET_ARRIVE = 0x2000000,
		F_SND_USE_ASYNC_BUFFER			= 0x3000000
	};

	class SocketEvent;

	class Socket:
		public SocketBase,
		public core::Dispatcher_Abstract<SocketEvent>
	{

	public:
		typedef Dispatcher_Abstract<SocketEvent> DispatcherT;

		explicit Socket(int const& fd, SocketAddr const& addr, SocketMode const& sm, SockBufferConfig const& sbc ,Family const& family , SockT const& type, Protocol const& proto, Option const& option = OPTION_NONE ):
			SocketBase(fd,addr,sm,sbc,family,type,proto,option),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0)
		{
			_Init();
		}

		explicit Socket( SocketAddr const& addr,Family const& family, SockT const& type, Protocol const& proto, Option const& option = OPTION_NONE ):
			SocketBase(addr,family,type,proto,option),
			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0)
		{
			_Init();
		}

		explicit Socket( SocketAddr const& addr, SockBufferConfig const& sbc ,Family const& family, SockT const& type, Protocol const& proto, Option const& option = OPTION_NONE ):
			SocketBase(addr,sbc,family,type,proto,option),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0)
		{
			_Init();
		}

		explicit Socket(SockBufferConfig const& sbc ,Family const& family, SockT const& type, Protocol const& proto, Option const& option = OPTION_NONE ):
			SocketBase(sbc,family,type,proto,option),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0)
		{
			_Init();
		}

		~Socket() {
			_Deinit();
		}

		void _Init() ;
		void _Deinit() ;

		//u64_t Test_DP_ADDRESS_TAG();
		int Close(int const& ec=0);
		int Shutdown(int const& flag, int const& ec=0);

		u32_t Accept( WWRP<Socket> sockets[], u32_t const& size, int& ec_o ) ;

		u32_t Send( byte_t const* const buffer, u32_t const& size, int& ec_o, int const& block_time = __SEND_BLOCK_TIME__, int const& flags = F_SND_USE_ASYNC_BUFFER) ;
		u32_t Recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0 ) ;

		u32_t Pump(int& ec_o, int const& flag = 0);

		void HandleAsyncPump();

		u32_t Flush(u32_t& left, int& ec_o, int const& block_time = __FLUSH_BLOCK_TIME__ /* in milliseconds , -1 == INFINITE */ ) ;
		u32_t HandleAsyncFlush(u32_t& left, int& ec_o, int const& block_time = __ASYNC_FLUSH_BLOCK_TIME /* in milliseconds, -1 == INFINITE*/);

		int SendPacket(WWSP<wawo::algorithm::Packet> const& packet, int const& flags = F_SND_USE_ASYNC_BUFFER);
		u32_t ReceivePackets(WWSP<wawo::algorithm::Packet> arrives[], u32_t const& size, int& ec_o, int const& flag= F_RCV_BLOCK_UNTIL_PACKET_ARRIVE );

		//you must Lock( Socket::L_READ_BUFFER ); for mul thread safety
		inline wawo::algorithm::BytesRingBuffer* const& GetRBuffer() const {
			WAWO_ASSERT( m_rb != NULL );
			return m_rb ;
		}

		//you must Lock( Socket::L_WRITE_BUFFER ); for mul thread safety
		inline wawo::algorithm::BytesRingBuffer* const& GetWBuffer() const {
			WAWO_ASSERT( m_rb != NULL );
			return m_sb;
		}

		inline bool IsFlushTimerExpired() {
			if (m_async_wt == 0) return false;

			LockGuard<SpinMutex> lg(m_mutexes[L_WRITE]);
			u64_t current = wawo::time::curr_milliseconds();
			return ( (m_sb->BytesCount()>0)&& (m_async_wt != 0) && ((current - m_async_wt) > m_delay_wp));
		}

		inline bool HasDataToFlush() {
			return (m_sb->BytesCount() > 0) ;
		}

		virtual int TLP_Handshake(bool const& nonblocking = false);

	private:
		u32_t _ReceivePackets(WWSP<wawo::algorithm::Packet> arrives[], u32_t const& size, int& ec_o, int const& flag = F_RCV_BLOCK_UNTIL_PACKET_ARRIVE);
		u32_t _Pump(int& ec_o,int const& flag = 0);

		inline void __CheckShutdownEvent(int const& added_flag, int const& ec, WWRP<SocketEvent>& evt_rd, WWRP<SocketEvent>& evt_wr) {
			WAWO_ASSERT( IsDataSocket() );
			if (added_flag == SHUTDOWN_RD) {
				evt_rd = WWRP<SocketEvent>(new SocketEvent(WWRP<Socket>(this), SE_RD_SHUTDOWN, core::Cookie(ec)));
			} else if (added_flag == SHUTDOWN_WR) {
				evt_wr = WWRP<SocketEvent>(new SocketEvent(WWRP<Socket>(this), SE_WR_SHUTDOWN, core::Cookie(ec)));
			} else if (added_flag == SHUTDOWN_RDWR) {
				evt_rd = WWRP<SocketEvent>(new SocketEvent(WWRP<Socket>(this), SE_RD_SHUTDOWN, core::Cookie(ec)));
				evt_wr = WWRP<SocketEvent>(new SocketEvent(WWRP<Socket>(this), SE_WR_SHUTDOWN, core::Cookie(ec)));
			} else {}
		}

		//must lock first, flush until left == 0 || get send block
		u32_t __Flush( u32_t& left, int& ec_o ) ;

		wawo::algorithm::BytesRingBuffer* m_sb; // buffer for send
		wawo::algorithm::BytesRingBuffer* m_rb; // buffer for recv

		byte_t* m_tsb; //tmp send buffer
		byte_t* m_trb; //tmp read buffer

		u32_t m_delay_wp;
		u64_t m_async_wt;
	};

}}
#endif
