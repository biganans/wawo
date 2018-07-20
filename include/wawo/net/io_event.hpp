#ifndef _WAWO_NET_IO_EVENT_HPP
#define _WAWO_NET_IO_EVENT_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

#include <functional>

namespace wawo { namespace net {
	
	enum async_io_op {
		AIO_READ,
		AIO_WRITE,
//		AIO_DIRECT_WRITE,
		AIO_ACCEPT,
		AIO_LISTEN,
		AIO_CONNECT
	};

	struct async_io_result {
		int op;
		union {
			int len;
			int code;
			int fd;
		} v;
		char* buf;
	};
	
	typedef std::function<void(async_io_result const& r)> fn_io_event;

#ifdef WAWO_IO_MODE_IOCP
	typedef std::function<int(void* ol)> fn_overlapped_io_event;
#endif
}}

#endif