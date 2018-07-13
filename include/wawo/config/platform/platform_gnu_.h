#pragma once
#ifndef _CONFIG_PLATFORM_WAWO_PLATFORM_GNU_H_
#define _CONFIG_PLATFORM_WAWO_PLATFORM_GNU_H_


//for gnu linux
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h> //sockaddr_un
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

#if WAWO_ADDRESSMODE_X64
	typedef unsigned long SOCKET ;
#else
	typedef unsigned int SOCKET;
#endif

#ifndef SOCKET_ERROR
	#define SOCKET_ERROR (-1)
#endif

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET  (SOCKET)(~0)
#endif


#define WAWO_CLOSE_SOCKET	::close
#define WAWO_DUP			dup
#define WAWO_DUP2			dup2

#endif