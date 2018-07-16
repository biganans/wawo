#include <wawo/net/address.hpp>

namespace wawo { namespace net {

	/**
	 * @param hostname, domain name of the host, example: www.google.com
	 * @param servicename, ftp, http, etc..
	 *		for a list of service names , u could refer to %WINDOW%/system32/drivers/etc/services on windows
	 *
	 */
	int get_addrinfo_by_host( char const* const hostname, char const* const servicename, std::vector<address>& addrs, int const& filter ) {

		WAWO_ASSERT( (hostname != NULL && wawo::strlen(hostname)) ||
					 (servicename != NULL && wawo::strlen(servicename))
					);

		struct addrinfo hint;
		::memset( &hint, 0, sizeof(hint));
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
			return retval;
		}

		for( ptr=result;ptr!=NULL; ptr = ptr->ai_next ) {

			address addr;
			s_family f;
			switch( ptr->ai_family ) {
			case AF_UNSPEC:
				{
					f = F_AF_UNSPEC;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			case AF_INET:
				{
					f = F_AF_INET;

					char addrv4_cstr[16] = {0};
					struct sockaddr_in* addrv4 = (struct sockaddr_in*) ptr->ai_addr;
					const char* addr_in_cstr = inet_ntop(AF_INET, (void*)&(addrv4->sin_addr), addrv4_cstr, 16 );
					//char* addrv4_cstr = inet_ntoa( addrv4->sin_addr );

					if(addr_in_cstr != NULL ) {
						addr = address(addrv4_cstr,0, f);
						addrs.push_back(addr);
					}
				}
				break;
			case AF_INET6:
				{
					f = F_AF_INET6;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			default:
				{
					f = F_AF_UNSPEC;
					//WAWO_ASSERT( !"to impl" );
				}
				break;
			}
		}
		freeaddrinfo(result);
		return wawo::OK;
	}


	extern int get_one_ipaddr_by_host( const char* hostname, std::string& ip_o, int const& filter ) {
		std::vector<address> infos;
		int retval = get_addrinfo_by_host( hostname, "", infos, filter );

		if( retval != 0 ) {
			return retval;
		}

		WAWO_ASSERT( infos.size() != 0 );
		ip_o = infos[0].dotip() ;
		return wawo::OK;
	}

	int hosttoip( const char* hostname, ipv4_t& nip ) {
		WAWO_ASSERT( strlen(hostname) > 0 );
		std::string dotip;
		int cret = get_one_ipaddr_by_host( hostname, dotip, 0 );
		WAWO_RETURN_V_IF_NOT_MATCH( cret, cret==wawo::OK );
		return dotiptoip(dotip.c_str(), nip);
	}

	int dotiptoip( const char* ipaddr, ipv4_t& ip ) {
		WAWO_ASSERT( strlen(ipaddr) > 0 );
		struct in_addr inaddr;
		int rval = ::inet_pton( AF_INET, ipaddr, &inaddr );
		if (rval == 0) return wawo::E_INVAL;
		if (rval == -1) { return (wawo::socket_get_last_errno()); }
		ip = ::ntohl(inaddr.s_addr);
		return wawo::OK;
	}

	bool is_dotipv4_decimal_notation(const char* cstr) {
		struct in_addr inaddr;
		int rval = ::inet_pton(AF_INET, cstr, &inaddr);
		if (rval == 0) return false;

		std::vector<std::string> vectors;
		wawo::split(std::string(cstr), ".", vectors);

		if (vectors.size() != 4) return false;

		char _tmp[256] = { 0 };
		snprintf(_tmp, 256, "%u.%u.%u.%u", wawo::to_u32(vectors[0].c_str()), wawo::to_u32(vectors[1].c_str()), wawo::to_u32(vectors[2].c_str()), wawo::to_u32(vectors[3].c_str()));

		return wawo::strcmp(_tmp, cstr) == 0;
	}

	std::string ipv4todotip(ipv4_t const& ip) {
		in_addr ia;
		ia.s_addr = ::htonl(ip);
		char addr[16] = { 0 };
		const char* addr_cstr = ::inet_ntop(AF_INET, &ia, addr, 16);
		if (addr_cstr != NULL) {
			return std::string(addr);
		}
		return std::string();
	}



	#ifdef WAWO_PLATFORM_GNU
	address::address( sockaddr_un& sockAddr_un ):
		m_identity(0)
	{
		memset( m_unixDomainPath, 0, sizeof(m_unixDomainPath)/sizeof(char) );
		strcpy( m_unixDomainPath, sockAddr_un.sun_path ) ;
	}
	#endif

	address::address():
		m_ipv4(0),
		m_port(0),
		m_family(F_AF_UNSPEC)
	{
	}

	address::address( char const* ip, unsigned short port , s_family const& f):
		m_ipv4(0),
		m_port(0),
		m_family(f)
	{
		WAWO_ASSERT( ip != NULL && wawo::strlen(ip) );
		//WAWO_ASSERT( port != 0 );

		ipv4_t ip_ = 0;
		int cret = dotiptoip(ip, ip_);
		WAWO_ASSERT( cret == wawo::OK );

		if( cret == wawo::OK ) {
			m_ipv4 = ip_;
			m_port = port;
		}
	}

	address::address( sockaddr_in const& sockaddr_in_ ):
		m_ipv4(::ntohl(sockaddr_in_.sin_addr.s_addr)),
		m_port(::ntohs(sockaddr_in_.sin_port)),
		m_family(OS_DEF_to_WAWO_DEF_family(sockaddr_in_.sin_family))
	{
	}

	address::~address() {
	}

	const std::string address::dotip() const {
		return ipv4todotip(m_ipv4);
	}

	std::string address::info() const {
		char info[32] = { 0 };
		int rtval = snprintf(const_cast<char*>(info), sizeof(info) / sizeof(info[0]), "%s:%d", dotip().c_str(), hport());
		WAWO_ASSERT(rtval > 0);
		(void)rtval;
		return std::string(info, wawo::strlen(info));
	}

}}