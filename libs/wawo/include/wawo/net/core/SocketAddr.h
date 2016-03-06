#ifndef _WAWO_NET_CORE_SOCKET_ADDR_H_
#define _WAWO_NET_CORE_SOCKET_ADDR_H_

#include <wawo/core.h>

namespace wawo { namespace net { namespace core {

	enum IpType {
		T_IPV4,
		T_IPV6
	};

	struct IpInfo {
		IpType type;
		char address[128]; //IPV4 32, IPV6 128
		uint32_t address_len;
	};

	namespace ipv4 {
		typedef unsigned long Ip;
		typedef unsigned short Port;
	}

	class SocketAddr {

	private:
		//unsigned long m_ulongIp; //host format of ip
		//unsigned short m_port; //host sequence of port
		uint64_t m_identity;

		char m_info[32]; //for tmp,,
#ifdef WAWO_PLATFORM_POSIX
		char m_unixDomainPath[256];
#endif

	public:

		static const char* ConvertToIpAddrFromHostName( const char* host_name );
		static unsigned long ConvertToHostSequenceUlongIpFromHostName( const char* host_name );
		static unsigned long ConvertToHostSequenceUlongIpFromIpAddr( const char* host_name );

#ifdef WAWO_PLATFORM_POSIX
		SocketAddr( sockaddr_un& sockAddr ) ;
#endif

		explicit SocketAddr();
		explicit SocketAddr( uint64_t const& identity );
		explicit SocketAddr( const char* ip, unsigned short port );
		explicit SocketAddr( sockaddr_in const& sockAddr ) ;

		~SocketAddr();

		inline bool IsNullAddr() const {return 0 == m_identity ;}
		inline uint64_t const& Identity() const { return m_identity; }

		bool operator == ( SocketAddr const& addr ) const {
			return m_identity == addr.Identity() ;
		}

		bool operator != ( SocketAddr const& addr ) const {
			return m_identity != addr.Identity() ;
		}

		bool operator < (SocketAddr const& addr) const {
			return Identity() < addr.Identity();
		}
		bool operator > (SocketAddr const& addr) const {
			return Identity() > addr.Identity();
		}

		char const* GetIpAddress() ;
		char const* GetIpAddress() const ;

		inline ipv4::Ip GetHostSequenceUlongIp() const {
			return ((m_identity>>16)&0xFFFFFFFF);
		}
		inline ipv4::Port GetHostSequencePort() const {
			return (m_identity&0xFFFF);
		}
		inline ipv4::Port GetNetSequencePort() const {
			return htons( GetHostSequencePort() );
		}
		inline ipv4::Ip GetNetSequenceUlongIp() const {
			return htonl( GetHostSequenceUlongIp());
		}

		inline void SetHostSequenceUlongIp( ipv4::Ip ulongIp ) {
			ipv4::Port port = (m_identity&0xFFFF);

			m_identity = 0;
			m_identity = ( m_identity|ulongIp)<<16 ;
			m_identity = (m_identity|port);
		}

		inline void SetHostSequencePort( ipv4::Port port ) {
			ipv4::Ip current_ip = (m_identity>>16)&0xFFFFFFFF;

			m_identity = 0;
			m_identity = (m_identity|current_ip)<<16;
			m_identity = (m_identity|(port&0xFFFF));
		}

		char const* const AddressInfo() const {
			memset( const_cast<char*>(m_info),0,sizeof(m_info)/sizeof(m_info[0])) ;
			snprintf( const_cast<char*>(m_info), sizeof(m_info)/sizeof(m_info[0]), "%s:%d", GetIpAddress(), GetHostSequencePort() );
			return &m_info[0] ;
		}

	};

}}}

#endif //_SOCKET_ADDR_H