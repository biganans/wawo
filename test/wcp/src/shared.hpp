#ifndef SHARED_HPP
#define SHARED_HPP

#include <wawo.h>

namespace wcp_test {
	enum FileTransferState {
		C_REQUEST,
		C_RECEIVE_HEADER,
		C_RECEIVE_CONTENT,
		C_RECEIVE_DONE
	};

	enum FileTransferCommand {
		C_TRANSFER_FILE ,
		C_TRANSFER_FILE_HEADER,
	};
}

#define TEST_PROTOCOL_WCP 1
#define TEST_FILE 99
#define FAST_TRANSFER 1

wawo::net::socket_buffer_cfg sbc = { 512 * 1024, 512 * 1024 };

#endif