
#include <wawo.h>
#include "shared.hpp"

int main(int argc, char** argv) {

	wawo::app _app;

	wawo::net::socket_addr remote_addr;
	remote_addr.so_family = wawo::net::F_AF_INET;

	if (argc == 2) {
		remote_addr.so_type = wawo::net::ST_STREAM;
		remote_addr.so_protocol = wawo::net::P_TCP;
	}
	else {
		remote_addr.so_type = wawo::net::ST_DGRAM;
		remote_addr.so_protocol = wawo::net::P_WCP;
		}

connect:
	remote_addr.so_address = wawo::net::address("127.0.0.1", 32310);
//	remote_addr.so_address = wawo::net::address("192.168.2.170", 32310);

	WWRP<wawo::net::socket> so = wawo::make_ref<wawo::net::socket>(sbc, remote_addr.so_family, remote_addr.so_type, remote_addr.so_protocol);

	int openrt = so->open();
	WAWO_RETURN_V_IF_NOT_MATCH(openrt, openrt == wawo::OK);

	int rt = so->connect(remote_addr.so_address);
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	WWRP<wawo::net::tlp_abstract> tlp = wawo::make_ref<typename StreamPeer::TLPT>();
	so->set_tlp(tlp);
	rt = so->tlp_handshake();
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::E_TLP_HANDSHAKE_DONE);
	WAWO_ASSERT(so->is_connected());

	WWRP<StreamPeer> peer = wawo::make_ref<StreamPeer>();
	peer->attach_socket(so);

//#define TEST_CLOSE
#ifdef TEST_CLOSE
	peer->close();
	wawo::this_thread::sleep(1);
	peer->detach_socket();
	peer = NULL;
	//while (1) {
	//	wawo::this_thread::sleep(1);
	//};
	goto connect;
#endif

#if TEST_FILE<8
	FILE* fp = fopen("../FILE_7", "rb");
#elif TEST_FILE<63
	FILE* fp = fopen("../FILE_63", "rb");
#else
	FILE* fp = fopen("../FILE", "rb");
#endif

	WAWO_ASSERT(fp != NULL);
	//long begin = ftell(fp);
	int seekrt = fseek(fp, 0L, SEEK_END);
	long end = ftell(fp);
	int seekbeg = fseek(fp, 0L, SEEK_SET);
	(void)seekrt;
	(void)seekbeg;

	wawo::u32_t flen = end;
	wawo::byte_t* file_content = new wawo::byte_t[flen];
	::size_t rbytes = fread((char*)file_content, 1, flen, fp);
	int fclosert = fclose(fp);

	WWSP<wawo::packet> received_packet = wawo::make_shared < wawo::packet>(flen);
	wawo::u32_t file_len;
	int transfer_state = wcp_test::C_REQUEST;

	WWSP<wawo::packet> inpack_tmp = wawo::make_shared<wawo::packet>( 256*1024);

	do {
		int ec;
		WWSP<StreamPeer::message_t> arrives[1];

		switch (transfer_state) {
		case wcp_test::C_REQUEST:
		{
			WWSP<wawo::packet> outp = wawo::make_shared<wawo::packet>();
			outp->write<wawo::u8_t>(wcp_test::C_TRANSFER_FILE);

			WWSP<StreamPeer::message_t> outm = wawo::make_shared<StreamPeer::message_t>(outp);
			int sndrt = peer->do_send_message(outm);
			WAWO_ASSERT(sndrt == wawo::OK);

			transfer_state = wcp_test::C_RECEIVE_HEADER;
			inpack_tmp->reset();
		}
		break;
		case wcp_test::C_RECEIVE_HEADER:
		{
			while (inpack_tmp->len() < (sizeof(wawo::u8_t) + sizeof(wawo::u32_t)) ) {
				wawo::u32_t count = peer->do_receive_messasges(arrives, 1, ec);
				WAWO_ASSERT(ec == wawo::OK || ec == wawo::E_TLP_TRY_AGAIN);
				WAWO_ASSERT(count == 1);
				inpack_tmp->write( arrives[0]->data->begin(), arrives[0]->data->len() );
			}

			wawo::u8_t cmd = inpack_tmp->read<wawo::u8_t>();
			WAWO_ASSERT(cmd == wcp_test::C_TRANSFER_FILE_HEADER);

			file_len = inpack_tmp->read<wawo::u32_t>();
			transfer_state = wcp_test::C_RECEIVE_CONTENT;
		}
		break;
		case wcp_test::C_RECEIVE_CONTENT:
		{
			while ( received_packet->len() != file_len ) {

				if (inpack_tmp == NULL || inpack_tmp->len() == 0) {
					wawo::u32_t count = peer->do_receive_messasges(arrives, 1, ec);
					WAWO_ASSERT(ec == wawo::OK || ec == wawo::E_TLP_TRY_AGAIN);
					WAWO_ASSERT(count == 1);
					inpack_tmp = arrives[0]->data;
				}

				wawo::u32_t file_len_left = (file_len - received_packet->len());
				if (inpack_tmp->len() >= file_len_left ) {
					received_packet->write(inpack_tmp->begin(), file_len_left);
					inpack_tmp->skip(file_len_left);
				}
				else {
					received_packet->write(inpack_tmp->begin(), inpack_tmp->len());
					inpack_tmp->reset();
				}
			}

			transfer_state = wcp_test::C_RECEIVE_DONE;
		}
		break;
		case wcp_test::C_RECEIVE_DONE:
		{
			WAWO_ASSERT(file_len == flen);

			if (wawo::strncmp((char*)file_content, (char*)received_packet->begin(), flen) != 0) {
				WAWO_ASSERT("file transfer failed");
			}

#if FAST_TRANSFER
			transfer_state = wcp_test::C_RECEIVE_HEADER;
#else
			transfer_state = wcp_test::C_REQUEST;
#endif
			received_packet->reset();
		}
		break;
		}

	} while (true);
}