#include <wawo/net/SocketAddr.hpp>
#include <wawo/log/LoggerManager.h>

namespace wawo { namespace net {


	/**
	 * @param hostname, domain name of the host, example: www.google.com
	 * @param servicename, ftp, http, etc..
	 *		for a list of service names , u could refer to %WINDOW%/system32/drivers/etc/services on windows
	 *
	 */
	int GetAddrInfoByHost( char const* const hostname, char const* const servicename, std::vector<AddrInfo>& addrs, int const& filter ) {

		WAWO_ASSERT( (hostname != NULL && wawo::strlen(hostname)) ||
					 (servicename != NULL && wawo::strlen(servicename))
					);

		struct addrinfo hint;
		memset( &hint, 0, sizeof(hint));
		struct addrinfo* result = NULL;
		struct addrinfo* ptr = NULL;

		if( filter != 0 ) {
			int aif_family = (filter&(AIF_F_INET|AIF_F_INET6));
			if( aif_family == (AIF_F_INET) ) {
				hint.ai_family = AF_INET; //
			} else if( aif_family == AIF_F_INET6 ) {
				hint.ai_family = AF_INET6;
			} else {
				hint.ai_family = AF_UNSPEC;
			}

			int aif_sockt = (filter&(AIF_ST_DGRAM|AIF_ST_STREAM));
			if( aif_sockt == AIF_ST_STREAM) {
				hint.ai_socktype = SOCK_STREAM;
			} else if( aif_sockt == AIF_ST_DGRAM ) {
				hint.ai_socktype = SOCK_DGRAM;
			} else {
				hint.ai_socktype = 0;
			}

			int aif_protocol = (filter&(AIF_P_TCP|AIF_P_UDP));
			if( aif_protocol == AIF_P_TCP) {
				hint.ai_protocol = IPPROTO_TCP;
			} else if( aif_protocol == AIF_P_UDP ) {
				hint.ai_protocol = IPPROTO_UDP;
			} else {
				hint.ai_protocol = 0;
			}
		}

#ifdef WAWO_PLATFORM_WIN
		DWORD retval;
#else
		int retval;
#endif

		retval = getaddrinfo( hostname, servicename, &hint, &result );

		if( retval != 0 ) {
			WAWO_FATAL("[socketaddr]getaddrinfo failed: %d", wawo::SocketGetLastErrno() );
			return retval;
		}

		for( ptr=result;ptr!=NULL; ptr = ptr->ai_next ) {

			AddrInfo info;

			switch( ptr->ai_family ) {
			case AF_UNSPEC:
				{
					info.family = F_AF_UNSPEC;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			case AF_INET:
				{
					info.family = F_AF_INET;

					char addrv4_cstr[16] = {0};
					struct sockaddr_in* addrv4 = (struct sockaddr_in*) ptr->ai_addr;
					const char* addr_in_cstr = inet_ntop(AF_INET, (void*)&(addrv4->sin_addr), addrv4_cstr, 16 );
					//char* addrv4_cstr = inet_ntoa( addrv4->sin_addr );

					if(addr_in_cstr != NULL ) {
						info.ip = Len_CStr(addrv4_cstr);
						addrs.push_back(info);
					}
				}
				break;
			case AF_INET6:
				{
					info.family = F_AF_INET6;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			default:
				{
					info.family = F_UNKNOWN;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			}
		}
		freeaddrinfo(result);
		return wawo::OK;
	}


	extern int GetOneIpAddrByHost( const char* hostname, Len_CStr& ip_o, int const& filter ) {

		std::vector<AddrInfo> infos;
		int retval = GetAddrInfoByHost( hostname, "", infos, filter );

		if( retval != 0 ) {
			return retval;
		}

		WAWO_ASSERT( infos.size() != 0 );
		ip_o = infos[0].ip.CStr() ;
		return wawo::OK;
	}

	int ConvertToNetSequenceUlongIpFromHostName( const char* hostname, ipv4::Ip& ip ) {
		WAWO_ASSERT( strlen(hostname) > 0 );
		Len_CStr ipaddr;
		int cret = GetOneIpAddrByHost( hostname, ipaddr, 0 );
		WAWO_RETURN_V_IF_NOT_MATCH( cret, cret==wawo::OK );

		return ConvertToNetSequenceUlongIpFromIpAddr(ipaddr.CStr(), ip);
	}

	int ConvertToNetSequenceUlongIpFromIpAddr( const char* ipaddr, ipv4::Ip& ip ) {
		WAWO_ASSERT( strlen(ipaddr) > 0 );

		struct in_addr inaddr;
		int rval = inet_pton( AF_INET, ipaddr, &inaddr );
		if (rval == 0) return wawo::E_INVALID_IP_ADDR;
		if (rval == -1) { return WAWO_NEGATIVE(wawo::SocketGetLastErrno()); }
		ip = (inaddr.s_addr);
		//ip = ntohl( inet_addr(ipaddr) ) ;
		return wawo::OK;
	}

	#ifdef WAWO_PLATFORM_GNU
	SocketAddr::SocketAddr( sockaddr_un& sockAddr_un ):
		m_identity(0)
	{
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

	SocketAddr::SocketAddr( u64_t const& identity):
		m_identity(identity)
	{
	}

	SocketAddr::SocketAddr( char const* ip, unsigned short port )
		:m_identity(0)
	{
		WAWO_ASSERT( ip != NULL && wawo::strlen(ip) );
		WAWO_ASSERT( port != 0 );

		ipv4::Ip ulip = 0;
		int cret = ConvertToNetSequenceUlongIpFromIpAddr(ip, ulip);
		WAWO_ASSERT( cret == wawo::OK );

		if( cret == wawo::OK ) {
			m_identity	= (m_identity | (ulip&0xFFFFFFFF)) << 16 ;
			m_identity	= (m_identity | ( htons(port)&0xFFFF)) ;
		}
		WAWO_ASSERT( m_identity != 0 );
	}

	SocketAddr::SocketAddr( sockaddr_in const& socketAddr )
		:m_identity(0)
	{
		m_identity	= (m_identity | (( socketAddr.sin_addr.s_addr ) & 0xFFFFFFFF))<< 16 ;
		m_identity	= (m_identity | (( socketAddr.sin_port ) & 0xFFFF )) ;
	}

	const Len_CStr SocketAddr::GetIpAddress() const {
		in_addr ia ;
		ia.s_addr = ((m_identity>>16)&0xFFFFFFFF);
		char addr[16] = {0};
		const char* addr_cstr = inet_ntop( AF_INET, &ia, addr, 16 );
		if (addr_cstr != NULL) {
			return Len_CStr(addr);
		}
		return Len_CStr();
	}

	Len_CStr SocketAddr::AddressInfo() const {
		char info[32] = { 0 };
		int rtval = snprintf(const_cast<char*>(info), sizeof(info) / sizeof(info[0]), "%s:%d", GetIpAddress().CStr(), GetHostSequencePort());
		WAWO_ASSERT(rtval > 0);
		(void)rtval;
		return Len_CStr(info, wawo::strlen(info));
	}

}}
