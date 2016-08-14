#ifndef _WAWO_ENV_ENV_IMPL_GNU_HPP_
#define _WAWO_ENV_ENV_IMPL_GNU_HPP_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>

#include <vector>
#include <wawo/net/SocketAddr.hpp>

namespace wawo { namespace env {

	class Env_Impl {

	public:
		Env_Impl() {}
		~Env_Impl() {}

		int GetLocalIpList( std::vector<wawo::net::AddrInfo>& addrs ) {

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

                wawo::net::Family wfamily = wawo::net::F_UNKNOWN ;


                if(  family == AF_INET ) {
					wfamily = wawo::net::F_AF_INET;
					s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST );
                } else if( family == AF_INET6 ) {
					wfamily = wawo::net::F_AF_INET6;
                	s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                } else {
                	continue;
                }

                if( s == 0) {
                	wawo::net::AddrInfo info;
                	info.family = wfamily;

					//memset( (void*)&info.address[0], 0, sizeof(info.address) );
					//memcpy(info.address, host, strlen(host));
					//info.address_len = strlen(host);

					info.ip = Len_CStr( host, wawo::strlen(host) );

					addrs.push_back( info );
//					memset( (void*)&host[0], 0, NI_MAXHOST);
                }

            }
            freeifaddrs(ifaddr);
            return wawo::OK;
		}

		int GetLocalComputerName(Len_CStr& name) {
			name = "to_do";
			return wawo::OK;
		}
	};
}}

#define WAWO_ENV (wawo::utils::Env::GetInstance())
#endif
