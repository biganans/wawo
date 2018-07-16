#ifndef _WAWO_NET_SOCKETADDR_HPP_
#define _WAWO_NET_SOCKETADDR_HPP_

#include <wawo/core.hpp>
#include <string>

namespace wawo { namespace net {

	enum s_family {
		F_PF_INET,
		F_AF_INET,
		F_AF_INET6,
		F_AF_UNIX,
		F_PF_UNIX,
		F_AF_NETBIOS,
		F_AF_UNSPEC,
		F_MAX
	};

	static USHORT OS_DEF_family[F_MAX] = {
		PF_INET,
		AF_INET,
		AF_INET6,
		AF_UNIX,
		PF_UNIX,
		AF_NETBIOS,
		AF_UNSPEC
	};

	enum s_type {
		T_STREAM,
		T_DGRAM,
		T_RAW,
		T_RDM,
		T_SEQPACKET,
		T_UNKNOWN,
		T_MAX
	};

	static int OS_DEF_sock_type[T_MAX] = {
		SOCK_STREAM,
		SOCK_DGRAM,
		SOCK_RAW,
		SOCK_RDM,
		SOCK_SEQPACKET,
		0
	};

	enum s_protocol {
		P_TCP,
		P_UDP,
		P_ICMP,
		P_IGMP,
		P_L2TP,
		P_SCTP,
		P_RAW,
		P_WCP,
		P_UNKNOWN,
		P_MAX
	};

	const static char* protocol_str[P_MAX] = {
		"TCP",
		"UDP",
		"ICMP",
		"IGMP",
		"L2TP",
		"SCTP",
		"RAW",
		"WCP",
		"UNKNOWN"
	};

	static int OS_DEF_protocol[P_MAX] = {
		IPPROTO_TCP,
		IPPROTO_UDP,
		IPPROTO_ICMP,
		IPPROTO_IGMP,
		IPPROTO_L2TP,
		IPPROTO_SCTP,
		IPPROTO_RAW,
		IPPROTO_UDP,
		0
	};

	typedef unsigned long ipv4_t;
	typedef unsigned short port_t;

	enum AddrInfoFilter {
		AIF_ALL			= 0x0, //return all
		AIF_F_INET		= 0x01, //return only ipv4
		AIF_F_INET6		= 0x02, //return only ipv6
		AIF_ST_STREAM	= 0x04,
		AIF_ST_DGRAM	= 0x08,
		AIF_P_TCP		= 0x10,
		AIF_P_UDP		= 0x20
	};

	struct address {
		s_family m_family;
		ipv4_t m_ipv4;
		port_t m_port;

#ifdef WAWO_PLATFORM_GNU
		char m_unixDomainPath[256];
#endif

#ifdef WAWO_PLATFORM_GNU
		address( sockaddr_un& sockaddr_un_ ) ;
#endif

		explicit address();
		explicit address(const char* ip, unsigned short port, s_family const& f = F_AF_UNSPEC );
		explicit address( sockaddr_in const& sockaddr_in_ ) ;

		~address();

		inline bool is_null() const {return 0 == m_ipv4 && 0 == m_port ;}

		inline bool operator == ( address const& addr ) const {
			return m_ipv4 == addr.m_ipv4 && m_port == addr.m_port && m_family == addr.m_family ;
		}

		inline bool operator != ( address const& addr ) const {
			return !((*this) == addr);
		}

		inline bool operator < (address const& addr) const {
			const u64_t idl = (m_ipv4 << 24 | m_port<<8 | m_family);
			const u64_t idr = (addr.m_ipv4 << 24 | addr.m_port<<8|m_family);
			return idl < idr;
		}
		inline bool operator > (address const& addr) const {
			return !(*this < addr);
		}

		inline s_family family() const {
			return m_family;
		}

		const std::string dotip() const ;

		inline ipv4_t ipv4() const {
			return m_ipv4;
		}
		inline ipv4_t hipv4() const {
			return ipv4();
		}
		inline ipv4_t nipv4() const {
			return ::htonl(m_ipv4);
		}
		inline port_t port() const {
			return m_port;
		}
		inline port_t hport() const {
			return port();
		}
		inline port_t nport() const {
			return ::htons(m_port);
		}
		inline void setipv4(ipv4_t ip) {
			m_ipv4 = ip;
		}
		inline void setport(port_t port) {
			m_port = port;
		}
		inline void setfamily(s_family f) {
			m_family = f;
		}
		std::string info() const;
	};

	extern int get_addrinfo_by_host(char const* const hostname, char const* const servicename, std::vector<address>& ips, int const& filter);
	extern int get_one_ipaddr_by_host(const char* hostname, std::string& ip, int const& filter);

	extern int hosttoip(const char* hostname, ipv4_t& ip);
	extern int dotiptoip(const char* dotip, ipv4_t& ip);
	extern bool is_dotipv4_decimal_notation(const char* string);

	extern std::string ipv4todotip(ipv4_t const& ip);

	struct socketaddr {
		inline socketaddr() :
			so_type(T_UNKNOWN),
			so_proto(P_UNKNOWN),
			so_address()
		{}

		inline socketaddr(address const& address):
			so_type(T_UNKNOWN),
			so_proto(P_UNKNOWN),
			so_address(address)
		{}

		inline socketaddr(s_type const& t, s_protocol const& p, address const& address) :
			so_type(t),
			so_proto(p),
			so_address(address)
		{}

		address so_address;
		s_type so_type;
		s_protocol so_proto;
	};

	inline s_family OS_DEF_to_WAWO_DEF_family(int f) {
		switch (f) {
		case AF_INET:
		{
			return F_AF_INET;
		}
		break;
		case AF_INET6:
		{
			return F_AF_INET6;
		}
		break;
		case AF_UNIX:
		{
			return F_AF_UNIX;
		}
		break;
		case AF_NETBIOS:
		{
			return F_AF_NETBIOS;
		}
		break;
		case AF_UNSPEC:
		default:
		{
			return F_AF_UNSPEC;
		}
		}
	}
}}
#endif //_ADDRESS_H