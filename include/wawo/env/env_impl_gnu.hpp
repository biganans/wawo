#ifndef _WAWO_ENV_ENV_IMPL_GNU_HPP_
#define _WAWO_ENV_ENV_IMPL_GNU_HPP_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>

#include <vector>
#include <wawo/net/address.hpp>

namespace wawo { namespace env {

	class env_impl {

	public:
		env_impl() {}
		~env_impl() {}

		int GetLocalIpList( std::vector<wawo::net::socket_addr>& addrs ) {

            struct ifaddrs *ifaddr;
            struct ifaddrs *ifa;

            int family, s;
            char host[NI_MAXHOST] ;

            if( getifaddrs(&ifaddr) == -1) {
            	return wawo::get_last_errno() ;
            }

            for( ifa = ifaddr; ifa != NULL ;ifa = ifa->ifa_next ) {

            	if(ifa->ifa_addr == NULL) {
            		continue;
            	}

                family = ifa->ifa_addr->sa_family;

                wawo::net::family wfamily = wawo::net::F_UNKNOWN ;


                if(  family == AF_INET ) {
					wfamily = wawo::net::F_AF_INET;
					s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST );
                } else if( family == AF_INET6 ) {
					wfamily = wawo::net::F_AF_INET6;
                	s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
					//@TODO
					continue;
                } else {
                	continue;
                }

                if( s == 0) {
                	wawo::net::socket_addr info;
                	info.so_family = wfamily;

					//memset( (void*)&info.address[0], 0, sizeof(info.address) );
					//memcpy(info.address, host, strlen(host));
					//info.address_length = strlen(host);

					info.so_address = wawo::net::address( host,0 );

					addrs.push_back( info );
//					memset( (void*)&host[0], 0, NI_MAXHOST);
                }

            }
            freeifaddrs(ifaddr);
            return wawo::OK;
		}

		int GetLocalComputerName(len_cstr& name) {
			name = "to_do";
			return wawo::OK;
		}
	};
}}

#define WAWO_ENV (wawo::utils::env::instance())
#endif
