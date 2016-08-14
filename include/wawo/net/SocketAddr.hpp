#ifndef _WAWO_NET_SOCKETADDR_HPP_
#define _WAWO_NET_SOCKETADDR_HPP_

#include <wawo/core.h>

namespace wawo { namespace net {

	enum Family {
		F_UNKNOWN,
		F_PF_INET,
		F_AF_INET,
		F_AF_INET6,
		F_AF_UNIX,
		F_PF_UNIX,
		F_AF_UNSPEC,
		F_AF_NETBIOS
	};

	enum SockT {
		ST_UNKNOWN,
		ST_STREAM,
		ST_DGRAM,
		ST_RAW,
		ST_RDM,
		ST_SEQPACKET
	};

	enum Protocol {
		P_UNKNOWN,
		P_TCP,
		P_UDP,
		P_ICMP,
		P_IGMP,
		P_L2TP,
		P_SCTP,
		P_RAW
	};

	namespace ipv4 {
		//network bytes sequence
		typedef unsigned long Ip;
		typedef unsigned short Port;
	}

	struct AddrInfo {

		AddrInfo():
			family(F_UNKNOWN),
			sockt(ST_UNKNOWN),
			protocol( P_UNKNOWN),
			ip(""),
			port(0)
		{}

		Family family;
		SockT sockt;
		Protocol protocol;

		Len_CStr ip;
		u16_t port;
	};

	enum AddrInfoFilter {
		AIF_ALL			= 0x0, //return all
		AIF_F_INET		= 0x01, //return only ipv4
		AIF_F_INET6		= 0x02, //return only ipv6
		AIF_ST_STREAM	= 0x04,
		AIF_ST_DGRAM	= 0x08,
		AIF_P_TCP		= 0x10,
		AIF_P_UDP		= 0x20
	};
	
	extern int GetAddrInfoByHost( char const* const hostname, char const* const servicename, std::vector<AddrInfo>& ips, int const& filter );
	extern int GetOneIpAddrByHost( const char* hostname, Len_CStr& ip, int const& filter );

	extern int ConvertToNetSequenceUlongIpFromHostName( const char* hostname, ipv4::Ip& ip );
	extern int ConvertToNetSequenceUlongIpFromIpAddr( const char* ipaddr, ipv4::Ip& ip );

	class SocketAddr {

	private:
		//unsigned long m_ulongIp; //host format of ip
		//unsigned short m_port; //host sequence of port
		u64_t m_identity;

#ifdef WAWO_PLATFORM_GNU
		char m_unixDomainPath[256];
#endif

	public:
#ifdef WAWO_PLATFORM_GNU
		SocketAddr( sockaddr_un& sockAddr ) ;
#endif

		explicit SocketAddr();
		explicit SocketAddr(const char* ip, unsigned short port);
		explicit SocketAddr( u64_t const& identity );
		explicit SocketAddr( sockaddr_in const& sockAddr ) ;

		~SocketAddr();

		inline bool IsNullAddr() const {return 0 == m_identity ;}
		inline u64_t const& Identity() const { return m_identity; }

		inline bool operator == ( SocketAddr const& addr ) const {
			return m_identity == addr.Identity() ;
		}

		inline bool operator != ( SocketAddr const& addr ) const {
			return m_identity != addr.Identity() ;
		}

		inline bool operator < (SocketAddr const& addr) const {
			return Identity() < addr.Identity();
		}
		inline bool operator > (SocketAddr const& addr) const {
			return Identity() > addr.Identity();
		}

		const Len_CStr GetIpAddress() const ;

		inline ipv4::Ip GetHostSequenceUlongIp() const {
			return ntohl(GetNetSequenceUlongIp());
		}
		inline ipv4::Port GetHostSequencePort() const {
			return ntohs(GetNetSequencePort());
		}
		inline ipv4::Port GetNetSequencePort() const {
			return ((m_identity&0xFFFF));
		}
		inline ipv4::Ip GetNetSequenceUlongIp() const {
			return (((m_identity>>16)&0xFFFFFFFF));
		}

		inline void SetNetSequenceUlongIp( ipv4::Ip const& ulongIp ) {
			ipv4::Port port = (m_identity&0xFFFF);
			m_identity = 0;
			m_identity = (m_identity|ulongIp)<<16 ;
			m_identity = (m_identity|port);
		}

		inline void SetNetSequencePort( ipv4::Port const& port ) {
			ipv4::Ip current_ip = (m_identity>>16)&0xFFFFFFFF;
			m_identity = 0;
			m_identity = (m_identity|current_ip)<<16;
			m_identity = (m_identity|(port&0xFFFF));
		}

		Len_CStr AddressInfo() const;
	};

}}
#endif //_SOCKET_ADDR_H