#include <wawo/net/core/SocketAddr.h>


namespace wawo { namespace net { namespace core {

	const char* SocketAddr::ConvertToIpAddrFromHostName( const char* host_name ) {
		struct hostent *addr_info ;
		addr_info = gethostbyname( host_name );

		if( addr_info == NULL ) {
			return "" ;
		}
		in_addr ia;
		memset( &ia, 0, sizeof(ia) );
		memcpy( (char*) &(ia.s_addr), (char*) addr_info->h_addr, addr_info->h_length ) ;
		return inet_ntoa( ia );
	}

	ipv4::Ip SocketAddr::ConvertToHostSequenceUlongIpFromHostName( const char* host_name ) {
		WAWO_ASSERT( strlen(host_name) > 0 );
		return ConvertToHostSequenceUlongIpFromIpAddr( ConvertToIpAddrFromHostName(host_name) ) ;
	}

	ipv4::Ip SocketAddr::ConvertToHostSequenceUlongIpFromIpAddr( const char* ip_addr ) {
		WAWO_ASSERT( strlen(ip_addr) > 0 );
		return ntohl( inet_addr(ip_addr) ) ;
	}

	#ifdef WAWO_PLATFORM_POSIX
	SocketAddr::SocketAddr( sockaddr_un& sockAddr_un ) {
		memset( m_unixDomainPath, 0, sizeof(m_unixDomainPath)/sizeof(char) );
		strcpy( m_unixDomainPath, sockAddr_un.sun_path ) ;	
	}
	#endif
	
	SocketAddr::SocketAddr()
		: m_identity(0)
	{
	}

	SocketAddr::~SocketAddr() {
	}
	SocketAddr::SocketAddr( uint64_t const& identity):
		m_identity(identity)
	{
	}

	SocketAddr::SocketAddr( char const* ip, unsigned short port )
		:m_identity(0) 
	{
		m_identity	= (m_identity | (ConvertToHostSequenceUlongIpFromIpAddr( ip )&0xFFFFFFFF)) << 16 ;
		m_identity	= (m_identity | ( port & 0xFFFF)) ;
	}

	SocketAddr::SocketAddr( sockaddr_in const& socketAddr ) 
		:m_identity(0)
	{
		m_identity	= (m_identity | (ntohl( socketAddr.sin_addr.s_addr ) & 0xFFFFFFFF))<< 16 ;
		m_identity	= (m_identity | (ntohs( socketAddr.sin_port ) & 0xFFFF )) ;
	}

	const char* SocketAddr::GetIpAddress() {
		in_addr ia ;

		ia.s_addr = htonl( ((m_identity>>16)&0xFFFFFFFF) ) ;
		return inet_ntoa( ia ) ;
	}
	const char* SocketAddr::GetIpAddress() const {
		in_addr ia ;

		ia.s_addr = htonl( ((m_identity>>16)&0xFFFFFFFF) ) ;
		return inet_ntoa( ia ) ;
	}

}}}