#ifndef _WAWO_UTILS_ENV_IMPL_WIN32_HPP_
#define _WAWO_UTILS_ENV_IMPL_WIN32_HPP_

#include <wawo/core.h>
#include <iphlpapi.h>
#include <wawo/log/LoggerManager.h>

#pragma comment(lib, "IPHlpApi.lib")
namespace wawo { namespace utils {

	class Env_Impl {

	public:
		Env_Impl() {
	
		}

		~Env_Impl() {
		}
		//return the ip count if success, and fill in parameter<ips> with local iplist
		int GetLocalIpList( std::vector<wawo::net::core::IpInfo>& ips ) {

			char _buffer[1024]= {0};
			DWORD _buffer_len = 1024;

			ULONG adapter_address_buffer_size = 1024*20; //20kb


			IP_ADAPTER_ADDRESSES* adapter_address(NULL);
			IP_ADAPTER_ADDRESSES* original_address(NULL);
			IP_ADAPTER_ADDRESSES* adapter(NULL);

			adapter_address = (IP_ADAPTER_ADDRESSES*) malloc(adapter_address_buffer_size);
			original_address = adapter_address; //for free

			WAWO_NULL_POINT_CHECK( adapter_address );

			ULONG flags = GAA_FLAG_SKIP_ANYCAST
				| GAA_FLAG_SKIP_MULTICAST
				| GAA_FLAG_SKIP_DNS_SERVER
				| GAA_FLAG_SKIP_FRIENDLY_NAME
				;

			flags = GAA_FLAG_SKIP_ANYCAST;

			DWORD error = ::GetAdaptersAddresses(AF_UNSPEC,flags,NULL, adapter_address, &adapter_address_buffer_size);

			if( ERROR_SUCCESS != error ) {

				free(original_address);
				original_address = NULL;
				WAWO_LOG_DEBUG("ENV", "::GetAdaptersAddresses() failed, error: %d", error );
				return wawo::GetLastErrno() ;
			}

			while( adapter_address ) {
		
				WAWO_LOG_DEBUG("ENV", "ip address info");
				WAWO_LOG_DEBUG("ENV", "%s, %.2x-%.2x-%.2x-%.2x-%.2x-%.2x: \n", 
				adapter_address->FriendlyName,
				adapter_address->PhysicalAddress[0],adapter_address->PhysicalAddress[1],
				adapter_address->PhysicalAddress[2],adapter_address->PhysicalAddress[3],
				adapter_address->PhysicalAddress[4],adapter_address->PhysicalAddress[5]
				);

				PIP_ADAPTER_UNICAST_ADDRESS pUnicast = adapter_address->FirstUnicastAddress;
				IP_ADAPTER_DNS_SERVER_ADDRESS* pDnsServer = adapter_address->FirstDnsServerAddress;

				if( pDnsServer ) {
					sockaddr_in* sa_in = (sockaddr_in*) pDnsServer->Address.lpSockaddr;
					memset(_buffer, 0,_buffer_len);
					WAWO_LOG_DEBUG("ENV", "dns: %s", inet_ntop( AF_INET, &(sa_in->sin_addr), _buffer, _buffer_len ) );
				}

				if(adapter_address->OperStatus == IfOperStatusUp) {
					WAWO_LOG_DEBUG("ENV", "interface up");
				} else {
					WAWO_LOG_DEBUG("ENV", "interface down");
				}

				int i=0;
				for( ; pUnicast != NULL;i++ ) {
					if( pUnicast->Address.lpSockaddr->sa_family == AF_INET ) {
						sockaddr_in* sa_in = (sockaddr_in*)(pUnicast->Address.lpSockaddr) ;
						memset(_buffer, 0,_buffer_len);
						inet_ntop(AF_INET, &(sa_in->sin_addr), _buffer, _buffer_len) ;
						WAWO_LOG_DEBUG("ENV", "ipv4: %s", _buffer );
						
						wawo::net::core::IpInfo info ;
#ifdef _DEBUG
						memset( &info, 0, sizeof(wawo::net::core::IpInfo) );
#endif
						info.type= wawo::net::core::T_IPV4;
						memcpy(info.address, _buffer, strlen(_buffer));
						info.address_len = strlen(_buffer);
						ips.push_back( info );

					} else if( pUnicast->Address.lpSockaddr->sa_family == AF_INET6 ) {
						sockaddr_in6* sa_in = (sockaddr_in6*)(pUnicast->Address.lpSockaddr) ;
						memset(_buffer, 0,_buffer_len);
						inet_ntop(AF_INET6, &(sa_in->sin6_addr), _buffer, _buffer_len) ;
						WAWO_LOG_DEBUG("ENV", "ipv6: %s", _buffer );

						wawo::net::core::IpInfo info;
#ifdef _DEBUG
						memset(&info,0,sizeof(wawo::net::core::IpInfo));
#endif
						if( strlen(_buffer) == 128 ) {
							info.type = wawo::net::core::T_IPV6;
							memcpy(info.address, _buffer, strlen(_buffer));
							info.address_len = strlen(_buffer);
							ips.push_back(info);
						}

					} else {
						WAWO_THROW_EXCEPTION("invalid AF familay");
					}

					pUnicast = pUnicast->Next;
				}

				adapter_address = adapter_address->Next;
			}

			free(original_address);

			return wawo::OK ;
		}
	
	};
}}
#endif