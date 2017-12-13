#ifndef _CONFIG_PLATFORM_WAWO_PLATFORM_WIN_H_
#define _CONFIG_PLATFORM_WAWO_PLATFORM_WIN_H_

#ifdef FD_SETSIZE //disable warning
	#undef FD_SETSIZE
#endif

#define FD_SETSIZE 1024 //will only affact MODE_SELECT

namespace wawo {
	typedef int SOCKET ;
}

//generic
#include <SDKDDKVer.h>
#include <winsock2.h> //WIN32_LEAN_AND_MEAN will exclude this from Windows.h
#include <WS2tcpip.h> //for inet_ntop, etc
#include <mstcpip.h>
#include <Windows.h> //will include winsock.h
#include <tchar.h>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")

//for shutdown
#define SHUT_RD		SD_RECEIVE
#define SHUT_WR		SD_SEND
#define SHUT_RDWR	SD_BOTH

#define WAWO_CLOSE_SOCKET	closesocket
#define WAWO_DUP			_dup
#define WAWO_DUP2			_dup2

#if _MSC_VER<1900
	#define snprintf	sprintf_s
#endif

#endif