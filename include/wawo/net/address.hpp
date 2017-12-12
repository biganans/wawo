#ifndef _WAWO_NET_SOCKETADDR_HPP_
#define _WAWO_NET_SOCKETADDR_HPP_

#include <wawo/core.hpp>

namespace wawo { namespace net {

	enum family {
		F_UNKNOWN,
		F_PF_INET,
		F_AF_INET,
		F_AF_INET6,
		F_AF_UNIX,
		F_PF_UNIX,
		F_AF_UNSPEC,
		F_AF_NETBIOS
	};

	enum sock_type {
		ST_UNKNOWN,
		ST_STREAM,
		ST_DGRAM,
		ST_RAW,
		ST_RDM,
		ST_SEQPACKET
	};

	enum protocol_type {
		P_UNKNOWN,
		P_TCP,
		P_UDP,
		P_ICMP,
		P_IGMP,
		P_L2TP,
		P_SCTP,
		P_RAW,
		P_WCP,
		P_MAX
	};

	extern const char* protocol_str[P_MAX];

	namespace ipv4 {
		//network bytes sequence
		typedef unsigned long Ip;
		typedef unsigned short Port;
	}

	enum AddrInfoFilter {
		AIF_ALL			= 0x0, //return all
		AIF_F_INET		= 0x01, //return only ipv4
		AIF_F_INET6		= 0x02, //return only ipv6
		AIF_ST_STREAM	= 0x04,
		AIF_ST_DGRAM	= 0x08,
		AIF_P_TCP		= 0x10,
		AIF_P_UDP		= 0x20
	};

	class address {
		//unsigned long m_ulongIp; //host format of ip
		//unsigned short m_port; //host sequence of port
		u64_t m_identity;

#ifdef WAWO_PLATFORM_GNU
		char m_unixDomainPath[256];
#endif

	public:
#ifdef WAWO_PLATFORM_GNU
		address( sockaddr_un& sockAddr ) ;
#endif

		explicit address();
		explicit address(const char* ip, unsigned short port);
		explicit address( u64_t const& identity );
		explicit address( sockaddr_in const& sockAddr ) ;

		~address();

		inline bool is_null() const {return 0 == m_identity ;}
		inline u64_t const& identity() const { return m_identity; }

		inline bool operator == ( address const& addr ) const {
			return m_identity == addr.identity() ;
		}

		inline bool operator != ( address const& addr ) const {
			return m_identity != addr.identity() ;
		}

		inline bool operator < (address const& addr) const {
			return identity() < addr.identity();
		}
		inline bool operator > (address const& addr) const {
			return identity() > addr.identity();
		}

		const len_cstr get_ip() const ;
		inline ipv4::Port get_port() const {
			return get_hostsequence_port();
		}

		inline ipv4::Ip get_hostsequence_ulongip() const {
			return ntohl(get_netsequence_ulongip());
		}
		inline ipv4::Port get_hostsequence_port() const {
			return ntohs(get_netsequence_port());
		}
		inline ipv4::Port get_netsequence_port() const {
			return ((m_identity&0xFFFF));
		}
		inline ipv4::Ip get_netsequence_ulongip() const {
			return (((m_identity>>16)&0xFFFFFFFF));
		}

		inline void set_netsequence_ulongip( ipv4::Ip const& ulongIp ) {
			ipv4::Port port = (m_identity&0xFFFF);
			m_identity = 0;
			m_identity = (m_identity|ulongIp)<<16 ;
			m_identity = (m_identity|port);
		}

		inline void set_netsequence_port( ipv4::Port const& port ) {
			ipv4::Ip current_ip = (m_identity>>16)&0xFFFFFFFF;
			m_identity = 0;
			m_identity = (m_identity|current_ip)<<16;
			m_identity = (m_identity|(port&0xFFFF));
		}

		len_cstr address_info() const;
	};

	struct socket_addr {

		socket_addr() :
			so_family(F_UNKNOWN),
			so_type(ST_UNKNOWN),
			so_protocol(P_UNKNOWN),
			so_address(0)
		{}

		family so_family;
		sock_type so_type;
		protocol_type so_protocol;

		address so_address;
	};


	extern int get_addrinfo_by_host(char const* const hostname, char const* const servicename, std::vector<socket_addr>& ips, int const& filter);
	extern int get_one_ipaddr_by_host(const char* hostname, len_cstr& ip, int const& filter);

	extern int convert_to_netsequence_ulongip_fromhost(const char* hostname, ipv4::Ip& ip);
	extern int convert_to_netsequence_ulongip_fromip(const char* ipaddr, ipv4::Ip& ip);

	extern bool is_ipv4_in_dotted_decimal_notation(const char* string);

}}
#endif //_SOCKET_ADDR_H
