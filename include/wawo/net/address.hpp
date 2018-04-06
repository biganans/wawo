#ifndef _WAWO_NET_SOCKETADDR_HPP_
#define _WAWO_NET_SOCKETADDR_HPP_

#include <wawo/core.hpp>

namespace wawo { namespace net {

	enum s_family {
		F_UNKNOWN,
		F_PF_INET,
		F_AF_INET,
		F_AF_INET6,
		F_AF_UNIX,
		F_PF_UNIX,
		F_AF_UNSPEC,
		F_AF_NETBIOS
	};

	enum s_type {
		T_UNKNOWN,
		T_STREAM,
		T_DGRAM,
		T_RAW,
		T_RDM,
		T_SEQPACKET
	};

	enum s_protocol {
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

		const len_cstr ip() const ;
		inline ipv4::Port port() const {
			return hport();
		}

		inline ipv4::Ip hip() const {
			return ::ntohl(nip());
		}
		inline ipv4::Port hport() const {
			return ::ntohs(nport());
		}
		inline ipv4::Port nport() const {
			return ((m_identity&0xFFFF));
		}
		inline ipv4::Ip nip() const {
			return (((m_identity>>16)&0xFFFFFFFF));
		}

		inline void setnip( ipv4::Ip const& ulongIp ) {
			ipv4::Port port = (m_identity&0xFFFF);
			m_identity = 0;
			m_identity = (m_identity|ulongIp)<<16 ;
			m_identity = (m_identity|port);
		}

		inline void setnport( ipv4::Port const& port ) {
			ipv4::Ip current_ip = (m_identity>>16)&0xFFFFFFFF;
			m_identity = 0;
			m_identity = (m_identity|current_ip)<<16;
			m_identity = (m_identity|(port&0xFFFF));
		}

		len_cstr info() const;
	};

	struct socketaddr {

		socketaddr() :
			so_family(F_UNKNOWN),
			so_type(T_UNKNOWN),
			so_protocol(P_UNKNOWN),
			so_address(0)
		{}

		s_family so_family;
		s_type so_type;
		s_protocol so_protocol;

		address so_address;
	};


	extern int get_addrinfo_by_host(char const* const hostname, char const* const servicename, std::vector<socketaddr>& ips, int const& filter);
	extern int get_one_ipaddr_by_host(const char* hostname, len_cstr& ip, int const& filter);

	extern int hostton(const char* hostname, ipv4::Ip& ip);
	extern int ipton(const char* ipaddr, ipv4::Ip& ip);

	extern bool is_ipv4_in_dotted_decimal_notation(const char* string);

}}
#endif //_SOCKET_ADDR_H
