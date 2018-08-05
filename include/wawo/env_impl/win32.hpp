#ifndef _WAWO_ENV_IMPL_WIN32_HPP_
#define _WAWO_ENV_IMPL_WIN32_HPP_

#include <wawo/core.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/net/address.hpp>
#include <iphlpapi.h>

#pragma comment(lib, "IPHlpApi.lib")
namespace wawo { namespace env_impl {

	class env_impl {

		int _get_local_interface_info(std::vector<wawo::net::address>& addrs, bool fetch_dns_server) {

			ULONG adapter_address_buffer_size = 1024 * 32; //20kb
			IP_ADAPTER_ADDRESSES* adapter_address(NULL);
			IP_ADAPTER_ADDRESSES* original_address(NULL);

			adapter_address = (IP_ADAPTER_ADDRESSES*) ::malloc(adapter_address_buffer_size);
			WAWO_ALLOC_CHECK(adapter_address, adapter_address_buffer_size);
			original_address = adapter_address; //for free

												/*
												ULONG flags = GAA_FLAG_SKIP_ANYCAST
												| GAA_FLAG_SKIP_MULTICAST
												| GAA_FLAG_SKIP_DNS_SERVER
												| GAA_FLAG_SKIP_FRIENDLY_NAME
												;
												*/

			ULONG flags = GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_DNS_SERVER;
			if (fetch_dns_server) {
				flags &= ~GAA_FLAG_SKIP_DNS_SERVER;
			}

			DWORD error = ::GetAdaptersAddresses(AF_INET, flags, NULL, adapter_address, &adapter_address_buffer_size);

			if (ERROR_SUCCESS != error) {
				::free(original_address);
				original_address = NULL;
				WAWO_WARN("[ENV]::GetAdaptersAddresses() failed, error: %d", error);
				return wawo::get_last_errno();
			}

			while (adapter_address) {

				//WAWO_DEBUG("[ENV]ip adress info, \n%s, %.2x-%.2x-%.2x-%.2x-%.2x-%.2x: \n", 
				//	adapter_address->FriendlyName,
				//	adapter_address->PhysicalAddress[0],adapter_address->PhysicalAddress[1],
				//	adapter_address->PhysicalAddress[2],adapter_address->PhysicalAddress[3],
				//	adapter_address->PhysicalAddress[4],adapter_address->PhysicalAddress[5]
				//);


				if (fetch_dns_server) {
					IP_ADAPTER_DNS_SERVER_ADDRESS* pDnsServer = adapter_address->FirstDnsServerAddress;
					while (pDnsServer) {
						sockaddr_in* sa_in = (sockaddr_in*)pDnsServer->Address.lpSockaddr;
						addrs.push_back(wawo::net::address(*sa_in));
						pDnsServer = pDnsServer->Next;
					}
					return wawo::OK;
				}

				PIP_ADAPTER_UNICAST_ADDRESS pUnicast = adapter_address->FirstUnicastAddress;
				while (pUnicast) {
					if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
						sockaddr_in* sa_in = (sockaddr_in*)(pUnicast->Address.lpSockaddr);
						//::memset(_buffer, 0,_buffer_length);
						//::inet_ntop(AF_INET, &(sa_in->sin_addr), _buffer, _buffer_length) ;
						wawo::net::address addr = wawo::net::address(*sa_in);
						addrs.push_back(addr);
					}
					else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
						continue;

						/*
						sockaddr_in6* sa_in = (sockaddr_in6*)(pUnicast->Address.lpSockaddr) ;
						::memset(_buffer, 0,_buffer_length);
						inet_ntop(AF_INET6, &(sa_in->sin6_addr), _buffer, _buffer_length) ;

						//wawo::net::socketaddr info;
						if( strlen(_buffer) == 128 ) {

						//info.so_family = wawo::net::F_AF_INET6;
						//info.so_address = wawo::net::address( _buffer, 0 );
						//addrs.push_back( info );
						}
						*/
					}
					else {
						WAWO_THROW("invalid AF familay");
					}

					pUnicast = pUnicast->Next;
				}
				adapter_address = adapter_address->Next;
			}

			::free(original_address);
			return wawo::OK;
		}

	public:
		env_impl() {
		}

		~env_impl() {
		}

		int get_local_dns_server_list(std::vector<wawo::net::address>& addrs) {
			return _get_local_interface_info(addrs, true);
		}

		int get_local_ip_list(std::vector<wawo::net::address>& addrs) {
			return _get_local_interface_info(addrs, false);
		}

		//refer to https://msdn.microsoft.com/en-us/library/windows/desktop/ms724295(v=vs.85).aspx
		int get_local_computer_name(std::string& name) {
			TCHAR infoBuf[256] = {0};
			DWORD bufSize = 256;

			BOOL rt = GetComputerName(infoBuf, &bufSize );
			WAWO_RETURN_V_IF_NOT_MATCH( WAWO_NEGATIVE(rt), rt != wawo::OK );

#ifdef _UNICODE
			// it returns ¨C1 cast to type size_t and sets errno to EILSEQ
			char mbBuf[512] = { 0 };
			::size_t size = std::wcstombs(mbBuf, infoBuf, 512 );
			if (size == (::size_t)-1) {
				return wawo::get_last_errno();
			}
			name = std::string(mbBuf, size);
			return wawo::OK;
#else
			name = std::string((const char*)infoBuf, (u32_t)bufSize);
			return wawo::OK;
#endif
		}
	};
}}
#endif