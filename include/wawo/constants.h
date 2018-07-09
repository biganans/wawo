#ifndef _WAWO_CONSTANTS_H_
#define _WAWO_CONSTANTS_H_

#include <climits>
#include <wawo/config/compiler.h>

namespace wawo {

	namespace limit {
		const i16_t INT8_MIN_V = (-127L - 1) ;
		const i16_t INT16_MIN_V = (-32767L - 1) ;
		const i32_t INT32_MIN_V = (-2147483647L - 1) ;
		const i64_t INT64_MIN_V = (-9223372036854775807LL - 1);

		const i8_t  INT8_MAX_V  = 127L ;
		const i16_t INT16_MAX_V = 32767L ;
		const i32_t INT32_MAX_V = 2147483647L ;
		const i64_t INT64_MAX_V = 9223372036854775807LL;

		const u8_t  UINT8_MAX_V  = 0xffUL ;
		const u16_t UINT16_MAX_V = 0xffffUL ;
		const u32_t UINT32_MAX_V = 0xffffffffUL ;
		const u64_t UINT64_MAX_V = 0xffffffffffffffffULL;
	}


	const int OK = 0;
	const int E_SOCKET_ERROR					= -1;
	const int E_EPERM							= -1; //operation not permitted
	const int E_ENOENT							= -2; //no such file or directory
	const int E_ESRCH							= -3; //no such process
	const int E_EINTR							= -4; //interrupted system call
	const int E_EIO								= -5; //I/O error
	const int E_ENXIO							= -6; //no such device or address
	const int E_E2BIG							= -7; //Arg list too long
	const int E_ENOEXEC							= -8; //Exec format error
	const int E_EBADF							= -9; //Bad file number
	const int E_ECHILD							= -10; //No child processes
	const int E_EAGAIN							= -11; //Try again
	const int E_ENOMEM							= -12; //Out of memory
	const int E_EACCESS							= -13; //Permission denied
	const int E_EFAULT							= -14; //Bad address
	const int E_ENOTBLK							= -15; //Block device required
	const int E_EBUSY							= -16; //Device or resource busy
	const int E_EEXIST							= -17; //File exists
	const int E_EEXDEV							= -18; //Cross-device link
	const int E_ENODEV							= -19; //No such device
	const int E_ENOTDIR							= -20; //Not a directory
	const int E_EISDIR							= -21; //Is a directory
	const int E_EINVAL							= -22; //Invalid argument
	const int E_ENFILE							= -23; //File table overflow
	const int E_EMFILE							= -24; //Too many open files
	const int E_ENOTTY							= -25; //Not a tty device
	const int E_ETXTBSY							= -26; //Text file busy
	const int E_EFBIG							= -27; //File too large
	const int E_ENOSPC							= -28; //No space left on device
	const int E_ESPIPE							= -29; //Illegal seek
	const int E_EROFS							= -30; //read-only file system
	const int E_EMLINK							= -31; //Too many links
	const int E_EPIPE							= -32; //broken pipe
	const int E_EDOM							= -33; //Math argument out of domain
	const int E_ERANGE							= -34; //Math result not representable
	const int E_EDEADLK							= -35; //dead lock wolld occur
	const int E_ENAMETOOLONG					= -36; //file too long
	const int E_ENOLCK							= -37; //No record locks available
	const int E_ENOSYS							= -38; //Function not implemented
	const int E_ENOTEMPTY						= -39; //dierctory not empty
	const int E_ELOOP							= -40; //too many symbolic links encountered
	const int E_EWOULDBLOCK						= -41; //same as EAGAIN
	const int E_ENOMSG							= -42; //No message of desired type
	const int E_EIDRM							= -43; //identifier removed
	const int E_ECHRNG							= -44; //channel number out of range
	const int E_EL2NSYNC						= -45; //level 2 not synchronized
	const int E_EL3HLT							= -46; //level 3 halted
	const int E_EL3RST							= -47; //level3 reset
	const int E_ELNRNG							= -48; //Link number out of range
	const int E_EUNATCH							= -49; //protocol driver not attached
	const int E_ENOCSI							= -50; //No CSI structure available
	const int E_EL2HLT							= -51; //Level 2 halted
	const int E_EBADE							= -52; //Invalid exchange
	const int E_EBADR							= -53; //Invalid request descriptor
	const int E_EXFULL							= -54; //Exchange full
	const int E_ENOANO							= -55; //No anode
	const int E_EBADRQC							= -56; //Invalid request code
	const int E_EBADSLT							= -57; //Invalid slot
	const int E_EDEADLOCK						= -58; //Same as EDEADLK
	const int E_EBFONT							= -59; //Bad font file format
	const int E_ENOSTR							= -60; //No data available
	const int E_ENODATA							= -61; //No data available
	const int E_ETIME							= -62; //Timer expired
	const int E_ENOSR							= -63; //Out of streams resources
	const int E_ERROR_NETNAME_DELETED			= -64;
	const int E_ENONET							= -64; //machine is not on network
	const int E_ENOPKG							= -65; //Package not installed
	const int E_EREMOTE							= -66; //Object is remote
	const int E_ENOLINK							= -67; //Link has been severed
	const int E_EADV							= -68; //Advertise error
	const int E_ESRMNT							= -69; //Srmount error
	const int E_ECOMM							= -70; //Communication error on send
	const int E_EPROTO							= -71; //protocol error
	const int E_EMULTIHOP						= -72; //Multihop attempted
	const int E_EDOTDOT							= -73; //RFS specific error
	const int E_EBADMSG							= -74; //Not a data message
	const int E_OVERFLOW						= -75; //Value too large for defined data type
	const int E_ENOTUNIQ						= -76; //Name not unique on network
	const int E_EBADFD							= -77; //File descriptor in bad state
	const int E_REMCHG							= -78; //Remote address changed
	const int E_ELIBACC							= -79; //Cannot access a needed shared library
	const int E_ELIBBAD							= -80; //Accessing a corrupted shared library
	const int E_ELIBSCN							= -81; //A .lib section in an .out is corrupted
	const int E_ELIMAX							= -82; //Linking in too many shared libraries
	const int E_ELIEXEC							= -83; //Cannot exec a shared library directly
	const int E_ELIILSEQ						= -84; //Illegal byte sequence
	const int E_ERESTART						= -85; //Interrupted system call should be restarted
	const int E_ESTRPIPE						= -86; //Streams pipe error
	const int E_EUSERS							= -87; //Too many users
	const int E_ENOTSOCK						= -88; //socket operation on non-socket
	const int E_EDESTADDRREQ					= -89; //Destination address required

	const int E_EMSGSIZE						= -90; //Message too long
	const int E_EPROTOTYPE						= -91; //protocol wrong type for socket
	const int E_ENOPROTOOPT						= -92; //protocol not available
	const int E_EPROTONOSUPPORT					= -93; //protocol not supported
	const int E_ESOCKTNOSUPPORT					= -94; //socket type not supported
	const int E_EOPNOTSUPP						= -95; //Operation not supported on transport
	const int E_EPFNOSUPPORT					= -96; //protocol family not supported
	const int E_EAFNOSUPPORT					= -97; //address family not supported by protocol
	const int E_EADDRINUSE						= -98; //address already in use
	const int E_EADDRNOTAVAIL					= -99; //Cannot assign requested address
	const int E_ENETDOWN						= -100; //Network is down
	const int E_ENETUNREACH						= -101; //Network is unreachable
	const int E_ENETRESET						= -102; //Network dropped
	const int E_ECONNABORTED					= -103; //Software caused connection
	const int E_ECONNRESET						= -104; //Connection reset by
	const int E_ENOBUFS							= -105; //No buffer space available
	const int E_EISCONN							= -106; //Transport endpoint
	const int E_ENOTCONN						= -107; //Transport endpoint
	const int E_ESHUTDOWN						= -108; //Cannot send after transport
	const int E_ETOOMANYREFS					= -109; //Too many references
	const int E_ETIMEOUT						= -110; //Connection timed
	const int E_ECONNREFUSED					= -111; //Connection refused
	const int E_EHOSTDOWN						= -112; //Host is down
	const int E_EHOSTUNREACH					= -113; //No route to host
	const int E_EALREADY						= -114; //Operation already
	const int E_EINPROGRESS						= -115; //Operation now in
	const int E_ESTALE							= -116; //Stale NFS file handle
	const int E_EUCLEAN							= -117; //Structure needs cleaning
	const int E_ENOTNAM							= -118; //Not a XENIX-named
	const int E_ENAVAIL							= -119; //No XENIX semaphores
	const int E_EISNAM							= -120; //Is a named type file
	const int E_EREMOTEIO						= -121; //Remote I/O error
	const int E_EDQUOT							= -122; //Quota exceeded
	const int E_ENOMEDIUM						= -123; //No medium found
	const int E_EMEDIUMTYPE						= -124; //Wrong medium type

	const int E_WAIT_TIMEOUT					= -258;
	const int E_ERROR_ABANDONED_WAIT_0			= -735;
	const int E_ERROR_IO_PENDING				= -997;
	const int E_ERROR_CONNECTION_ABORTED		= -1236;

// 1---19999; reserved for system call error code (windows)
	const int E_WSAEINTR						= -10004; // A blocking operation was interrupted by a call to WSACancelBlockingCall.
	const int E_WSAEBADF						= -10009; // The file handle supplied is not valid.
	const int E_WSAEINVAL						= -10022; // An invalid argument was supplied
	const int E_WSAEACCES						= -10013; // An attempt was made to access a socket in a way forbidden by its access permissions.
	const int E_WSAEMFILE						= -10024; // Too many open sockets.
	const int E_WSAEWOULDBLOCK					= -10035; // A non-blocking socket operation could not be completed immediately.
	const int E_WSAEINPROGRESS					= -10036; // A blocking operation is currently executing.
	const int E_WSAEALREADY						= -10037; // An operation was attempted on a non-blocking socket that already had an operation in progress.
	const int E_WSAENOTSOCK						= -10038; // An operation was attempted on something that is not a socket.
	const int E_WSAEMSGSIZE						= -10040; // A message sent on a datagram socket was larger than the internal message buffer or some other network limit;  or the buffer used to receive a datagram into was smaller than the datagram itself.
	const int E_WSAEPROTOTYPE					= -10041; //A protocol was specified in the socket function call that does not support the semantics of the socket type requested.
	const int E_WSAENOPROTOOPT					= -10042; //An unknown;  invalid;  or unsupported option or level was specified in a getsockopt or setsockopt call
	const int E_WSAEPROTONOSUPPORT				= -10043; //The requested protocol has not been configured into the system;  or no implementation for it exists.
	const int E_WSAEOPNOTSUPP					= -10045;  //The attempted operation is not supported for the type of object referenced.
	const int E_WSAEADDRINUSE					= -10048;  // Only one usage of each socket address (protocol/network address/port) is normally permitted.
	const int E_WSAEADDRNOTAVAIL				= -10049; // The requested address is not valid in its context.
	const int E_WSAENETDOWN						= -10050; //A socket operation encountered a dead network.
	const int E_WSAENETUNREACH					= -10051;  // A socket operation was attempted to an unreachable network.
	const int E_WSAENETRESET					= -10052; // The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.
	const int E_WSAECONNABORTED					= -10053; // An established connection was aborted by the software in your host machine.
	const int E_WSAECONNRESET					= -10054; // An existing connection was forcibly closed by the remote host.
	const int E_WSAENOBUFS						= -10055; // An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.
	const int E_WSAEISCONN						= -10056;  // A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
	const int E_WSAENOTCONN						= -10057; // A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
	const int E_WSAESHUTDOWN					= -10058;  //A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call
	const int E_WSAETOOMANYREFS					= -10059;  //Too many references to some kernel object.
	const int E_WSAETIMEDOUT					= -10060;  //A connection attempt failed because the connected party did not properly respond after a period of time;  or established connection failed because connected host has failed to respond.
	const int E_WSAECONNREFUSED					= -10061;  //No connection could be made because the target machine actively refused it.
	const int E_WSAELOOP						= -10062;  //Cannot translate name
	const int E_WSAEHOSTDOWN					= -10064; // A socket operation failed because the destination host was down
	const int E_WSAEHOSTUNREACH					= -10065; // A socket operation was attempted to an unreachable host.
	const int E_WSAEPROCLIM						= -10067; // A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously.
	const int E_WSAEUSERS						= -10068; 
	const int E_WSAEDQUOT						= -10069; 

	const int E_WSASYSNOTREADY					= -10091; 
	const int E_WSANOTINITIALISED				= -10093; 

	const int E_WSAEPROVIDERFAILEDINIT			= -10106; 
	const int E_WSASYSCALLFAILURE				= -10107; 
	const int E_WSASERVICE_NOT_FOUND			= -10108; 
	const int E_WSATYPE_NOT_FOUND				= -10109; 
	const int E_WSA_E_NO_MORE					= -10110; 
	const int E_WSA_E_CANCELLED					= -10111; 
	const int E_WSAEREFUSED						= -10112; 
	const int E_WSAHOST_NOT_FOUND				= -11001; 
	const int E_WSATRY_AGAIN					= -11002; 
	const int E_WSANO_RECOVERY					= -11003; 
	const int E_WSANO_DATA						= -11004; 
	const int E_WSA_QOS_RECEIVERS				= -11005; 
	const int E_WSA_QOS_SENDERS					= -11006; 

//internal error
	const int E_OPERATION_NOT_SUPPORTED	= -20001; 
	const int E_INVALID_OPERATION		= -20002; 
	const int E_INVALID_STATE			= -20003; 
	const int E_MEMORY_MALLOC_FAILED	= -20004; 
	const int E_MEMORY_ACCESS_ERROR		= -20005; 
	const int E_MEMORY_ALLOC_FAILED		= -20006; 
	const int E_MEMORY_NEW_FAILED		= -20007; 
	const int E_TRY_AGAIN				= -20008; 
	const int E_INVAL					= -20009;
	const int E_POLLER_FD_COUNT_LIMIT	= -20010;

	const int 	E_UNKNOWN					= -20010; 
	const int 	E_THREAD_INTERRUPT			= -20011; 
			
	const int 	E_XXTEA_INVALID_DATA		= -20012; 
	const int 	E_XXTEA_DECRYPT_FAILED		= -20013; 
	const int 	E_XXTEA_ENCRYPT_FAILED		= -20014; 

	const int 	E_CHANNEL_WRITE_BLOCK		= -21001; 
	const int 	E_CHANNEL_READ_BLOCK		= -21002; 

	const int 	E_CHANNEL_RD_SHUTDOWN_ALREADY = -21003; 
	const int 	E_CHANNEL_WR_SHUTDOWN_ALREADY = -21004; 

	const int 	E_CHANNEL_INVALID_STATE			= -21005; 
	const int 	E_CHANNEL_EXISTS				= -21006; 
	const int 	E_CHANNEL_CLOSED_ALREADY		= -21007; 
	const int	E_CHANNEL_WRITE_SHUTDOWNING		= -21008;

	const int 	E_SOCKET_GRACE_CLOSE					= -30000; 
	const int 	E_SOCKET_SEND_IO_BLOCK_EXPIRED			= -30001; 
	const int 	E_SOCKET_CONNECTING						= -30002; 

	const int 	E_SOCKET_INVALID_FAMILY					= -30012; 
	const int 	E_SOCKET_INVALID_TYPE					= -30013; 
	const int 	E_SOCKET_INVALID_ADDRESS				= -30014; 
	const int 	E_SOCKET_INVALID_PROTOCOL				= -30015; 
	const int 	E_SOCKET_UNKNOWN_ADDRESS_FORMAT			= -30016; 

	const int 	E_OBSERVER_EXIT							= -30023; 

	const int E_WCP_INTERNAL_ERROR						= -31000; 
	const int E_WCP_WPOLL_HANDLE_NOT_EXISTS				= -31001; 
	const int E_WCP_WPOLL_INVALID_OP					= -31002; 
	const int E_WCP_WCB_SHUTDOWNED						= -31003; 

	const int E_HANDLER_WEBSOCKET_CLOSED_ALREADY		= -32000; 

	const int E_HTTP_REQUEST_MISSING_OPTION				= -44001; 
	const int E_HTTP_MESSAGE_INVALID_TYPE				= -44002; 
	const int E_HTTP_REQUEST_NOT_DONE					= -44003; 
	const int E_HTTP_MISSING_HOST = -44004;

} //endif wawo ns

#endif //END OF WAWO_ERROR_HEADER