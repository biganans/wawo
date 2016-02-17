#ifndef _WAWO_ENV_ENV_IMPL_POSIX_HPP_
#define _WAWO_ENV_ENV_IMPL_POSIX_HPP_


#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>

#include <vector>
#include <wawo/net/core/SocketAddr.h>

namespace wawo { namespace utils {

	class Env_Impl {

	public:

		Env_Impl() {}
		~Env_Impl() {}

		int GetLocalIpList( std::vector<wawo::net::core::IpInfo>& ips ) {

            struct ifaddrs *ifaddr;
            struct ifaddrs *ifa;

            int family, s;
            char host[NI_MAXHOST] ;

            if( getifaddrs(&ifaddr) == -1) {
            	return wawo::GetLastErrno() ;
            }

            for( ifa = ifaddr; ifa != NULL ;ifa = ifa->ifa_next ) {


            	if(ifa->ifa_addr == NULL) {
            		continue;
            	}

                family = ifa->ifa_addr->sa_family;

                if(  family == AF_INET ) {
					s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST );
                } else if( family == AF_INET6 ) {
                	s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                } else {
                	continue;
                }

                if( s == 0) {

                	wawo::net::core::IpInfo info;

					info.type = (family == AF_INET) ? wawo::net::core::T_IPV4: wawo::net::core::T_IPV6 ;
					memset( (void*)&info.address[0], 0, sizeof(info.address) );
					memcpy(info.address, host, strlen(host));
					info.address_len = strlen(host);

					ips.push_back( info );
					memset( (void*)&host[0], 0, NI_MAXHOST);
                }

            }

            freeifaddrs(ifaddr);

            return wawo::OK;
		}

	};
}}

#define WAWO_ENV (wawo::utils::Env::GetInstance())
#endif
