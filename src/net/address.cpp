#include <wawo/net/address.hpp>
#include <wawo/log/logger_manager.h>

namespace wawo { namespace net {


	const char* protocol_str[P_MAX] = {
		"UNKNOWN",
		"TCP",
		"UDP",
		"ICMP",
		"IGMP",
		"L2TP",
		"SCTP",
		"RAW",
		"wcp"
	};


	/**
	 * @param hostname, domain name of the host, example: www.google.com
	 * @param servicename, ftp, http, etc..
	 *		for a list of service names , u could refer to %WINDOW%/system32/drivers/etc/services on windows
	 *
	 */
	int get_addrinfo_by_host( char const* const hostname, char const* const servicename, std::vector<socketaddr>& addrs, int const& filter ) {

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
			WAWO_ERR("[socketaddr]getaddrinfo failed: %d", wawo::socket_get_last_errno() );
			return retval;
		}

		for( ptr=result;ptr!=NULL; ptr = ptr->ai_next ) {

			socketaddr info;

			switch( ptr->ai_family ) {
			case AF_UNSPEC:
				{
					info.so_family = F_AF_UNSPEC;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			case AF_INET:
				{
					info.so_family = F_AF_INET;

					char addrv4_cstr[16] = {0};
					struct sockaddr_in* addrv4 = (struct sockaddr_in*) ptr->ai_addr;
					const char* addr_in_cstr = inet_ntop(AF_INET, (void*)&(addrv4->sin_addr), addrv4_cstr, 16 );
					//char* addrv4_cstr = inet_ntoa( addrv4->sin_addr );

					if(addr_in_cstr != NULL ) {
						info.so_address = address(addrv4_cstr,0);
						addrs.push_back(info);
					}
				}
				break;
			case AF_INET6:
				{
					info.so_family = F_AF_INET6;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			default:
				{
					info.so_family = F_UNKNOWN;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			}
		}
		freeaddrinfo(result);
		return wawo::OK;
	}


	extern int get_one_ipaddr_by_host( const char* hostname, len_cstr& ip_o, int const& filter ) {

		std::vector<socketaddr> infos;
		int retval = get_addrinfo_by_host( hostname, "", infos, filter );

		if( retval != 0 ) {
			return retval;
		}

		WAWO_ASSERT( infos.size() != 0 );
		ip_o = infos[0].so_address.ip() ;
		return wawo::OK;
	}

	int hostton( const char* hostname, ipv4::Ip& ip ) {
		WAWO_ASSERT( strlen(hostname) > 0 );
		len_cstr ipaddr;
		int cret = get_one_ipaddr_by_host( hostname, ipaddr, 0 );
		WAWO_RETURN_V_IF_NOT_MATCH( cret, cret==wawo::OK );
		return ipton(ipaddr.cstr, ip);
	}

	int ipton( const char* ipaddr, ipv4::Ip& ip ) {
		WAWO_ASSERT( strlen(ipaddr) > 0 );

		struct in_addr inaddr;
		int rval = inet_pton( AF_INET, ipaddr, &inaddr );
		if (rval == 0) return wawo::E_INVALID_DATA;
		if (rval == -1) { return WAWO_NEGATIVE(wawo::socket_get_last_errno()); }
		ip = (inaddr.s_addr);
		return wawo::OK;
	}

	bool is_ipv4_in_dotted_decimal_notation(const char* cstr) {
		struct in_addr inaddr;
		int rval = inet_pton(AF_INET, cstr, &inaddr);
		if (rval == 0) return false;

		std::vector<std::string> vectors;
		wawo::split(std::string(cstr), ".", vectors);

		if (vectors.size() != 4) return false;

		char _tmp[256] = { 0 };
		snprintf(_tmp, 256, "%u.%u.%u.%u", wawo::to_u32(vectors[0].c_str()), wawo::to_u32(vectors[1].c_str()), wawo::to_u32(vectors[2].c_str()), wawo::to_u32(vectors[3].c_str()));

		return wawo::strcmp(_tmp, cstr) == 0;
	}


	#ifdef WAWO_PLATFORM_GNU
	address::address( sockaddr_un& sockAddr_un ):
		m_identity(0)
	{
		memset( m_unixDomainPath, 0, sizeof(m_unixDomainPath)/sizeof(char) );
		strcpy( m_unixDomainPath, sockAddr_un.sun_path ) ;
	}
	#endif

	address::address()
		: m_identity(0)
	{
	}

	address::~address() {
	}

	address::address( u64_t const& identity):
		m_identity(identity)
	{
	}

	address::address( char const* ip, unsigned short port )
		:m_identity(0)
	{
		WAWO_ASSERT( ip != NULL && wawo::strlen(ip) );
		//WAWO_ASSERT( port != 0 );

		ipv4::Ip ulip = 0;
		int cret = ipton(ip, ulip);
		WAWO_ASSERT( cret == wawo::OK );

		if( cret == wawo::OK ) {
			m_identity	= (m_identity | (ulip&0xFFFFFFFF)) << 16 ;
			m_identity	= (m_identity | ( htons(port)&0xFFFF)) ;
		}
		WAWO_ASSERT( m_identity != 0 );
	}

	address::address( sockaddr_in const& socketAddr )
		:m_identity(0)
	{
		m_identity	= (m_identity | (( socketAddr.sin_addr.s_addr ) & 0xFFFFFFFF))<< 16 ;
		m_identity	= (m_identity | (( socketAddr.sin_port ) & 0xFFFF )) ;
	}

	const len_cstr address::ip() const {
		in_addr ia ;
		ia.s_addr = ((m_identity>>16)&0xFFFFFFFF);
		char addr[16] = {0};
		const char* addr_cstr = inet_ntop( AF_INET, &ia, addr, 16 );
		if (addr_cstr != NULL) {
			return len_cstr(addr);
		}
		return len_cstr();
	}

	len_cstr address::info() const {
		char info[32] = { 0 };
		int rtval = snprintf(const_cast<char*>(info), sizeof(info) / sizeof(info[0]), "%s:%d", ip().cstr, hport());
		WAWO_ASSERT(rtval > 0);
		(void)rtval;
		return len_cstr(info, wawo::strlen(info));
	}

}}
