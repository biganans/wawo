
#include <wawo.h>
#include "shared.hpp"

using namespace wcp_test;
typedef wawo::net::peer::cargo<wawo::net::tlp::stream> StreamPeer;



int main(int argc, char** argv) {


	wawo::app _app;
	WWRP<StreamNode> node = wawo::make_ref<StreamNode>();

	wawo::net::socket_addr listen_addr;
	listen_addr.so_address = wawo::net::address("0.0.0.0", 32310);
	listen_addr.so_family = wawo::net::F_AF_INET;

	if (argc == 2) {
		listen_addr.so_type = ST_STREAM;
		listen_addr.so_protocol = P_TCP;
	}
	else {
		listen_addr.so_type = ST_DGRAM;
		listen_addr.so_protocol = P_WCP;
	}

	int rt = node->start();
	(void)&rt;
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	int listenrt = node->start_listen(listen_addr, sbc);
	WAWO_RETURN_V_IF_NOT_MATCH(listenrt, listenrt == wawo::OK);

	_app.run_for();
	node->stop();

	WAWO_INFO("[roger]server exiting...");
	return 0;
}
