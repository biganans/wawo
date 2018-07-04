#ifndef _WAWO_NET_WINSOCK_HPP
#define _WAWO_NET_WINSOCK_HPP

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>

#include <mswsock.h>

namespace wawo { namespace net {
	enum winsock_api_ex {
		API_CONNECT_EX = 0,
		API_ACCEPT_EX,
		API_GET_ACCEPT_EX_SOCKADDRS,
		API_MAX
	};
	
	class winsock_helper : public wawo::singleton<winsock_helper> {
		DECLARE_SINGLETON_FRIEND(winsock_helper);
		void* m_api_address[API_MAX];

	protected:
		winsock_helper() {}
		~winsock_helper() {}

		void init_api_address() {
			GUID guid = WSAID_CONNECTEX;
			LPFN_CONNECTEX fn_connectEx;
			DWORD dwBytes;
			int fd = ::WSASocketW( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
			if (fd == SOCKET_ERROR) {
				WAWO_THROW("CREATE FD FAILED");
			}

			int loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guid, sizeof(guid),
				&fn_connectEx, sizeof(fn_connectEx),
				&dwBytes, NULL, NULL);

			if (loadrt != 0) {
				WAWO_THROW("load address: WSAID_CONNECTEX failed");
			}
			WAWO_ASSERT(fn_connectEx != 0);
			m_api_address[API_CONNECT_EX] = (void*)fn_connectEx;

			guid = WSAID_ACCEPTEX;
			LPFN_ACCEPTEX fn_acceptEx;
			loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guid, sizeof(guid),
				&fn_acceptEx, sizeof(fn_acceptEx),
				&dwBytes, NULL, NULL);

			if (loadrt != 0) {
				WAWO_THROW("load address: WSAID_ACCEPTEX failed");
			}
			WAWO_ASSERT(fn_acceptEx != 0);
			m_api_address[API_ACCEPT_EX] = (void*)fn_acceptEx;

			guid = WSAID_GETACCEPTEXSOCKADDRS;
			LPFN_GETACCEPTEXSOCKADDRS fn_getacceptexsockaddrs;
			loadrt = ::WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guid, sizeof(guid),
				&fn_getacceptexsockaddrs, sizeof(fn_getacceptexsockaddrs),
				&dwBytes, NULL, NULL);

			if (loadrt != 0) {
				WAWO_THROW("load address: WSAID_GETACCEPTEXSOCKADDRS failed");
			}
			WAWO_ASSERT(fn_getacceptexsockaddrs != 0);
			m_api_address[API_GET_ACCEPT_EX_SOCKADDRS] = (void*)fn_getacceptexsockaddrs;

			WAWO_CLOSE_SOCKET(fd);
		}

	public:
		void init() {
			WSADATA wsaData;
			int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (result != 0) {
				WAWO_THROW("INIT WINSOCK FAILED");
			}
			init_api_address();
		}

		void deinit() {
			::WSACleanup();
		}

		inline void* load_api_ex_address(winsock_api_ex id) {
			return m_api_address[id];
		}
	};
}}

#endif