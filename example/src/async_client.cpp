
#include <wawo.h>
#include "services/EchoClient.hpp"
#include "services/ServiceShare.h"

typedef wawo::net::peer::ros<> client_peer_t;
typedef services::EchoClient<client_peer_t> EchoProvider_Client;

class client_node :
	public wawo::net::ros_node<client_peer_t>
{
private:
	int m_max_peer;
	int m_curr_peer_count;

	wawo::net::socket_addr m_remote_addr;


public:
	client_node(int max_peer, wawo::net::socket_addr const& addr ) : m_max_peer(max_peer), m_curr_peer_count(0), m_remote_addr(addr) {		
	}

	void on_peer_close(WWRP<client_peer_t::peer_event_t> const& evt) {
		WAWO_LOG_INFO("client_node","peer closed");
	}

	void async_make_peer() {
		auto lambda_success = std::bind(&client_node::on_connect_success, WWRP<client_node>(this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		auto lambda_fail = std::bind(&client_node::on_connect_fail, WWRP<client_node>(this), std::placeholders::_1, std::placeholders::_2);
		int connrt = client_node::async_connect(m_remote_addr, WWRP<client_node>(this), lambda_success, lambda_fail);

		WAWO_INFO("[client_node]conn : %s, code: %d", connrt == wawo::OK ? "OK" : "failed" , connrt );
	}

	void on_connect_success(WWRP<client_peer_t> const& peer, WWRP<wawo::net::socket> const& so, WWRP<ref_base> const& cookie) {
	
		int rt = services::HelloRequestCallBack::ReqHello(peer);
		if (rt == wawo::OK) {
			++m_curr_peer_count;
		}

		if (m_curr_peer_count < m_max_peer) {
			async_make_peer();
		}
	}

	void on_connect_fail(int const& code, WWRP<ref_base> const& cookie) {
		WAWO_WARN("[client_node]conn failed: %d", code );
		async_make_peer();
	}
};

int main( int argc, char** argv ) {

	wawo::app application ;
	wawo::net::address remote_addr("127.0.0.1", 22310);

	wawo::net::socket_addr addr_info;
	addr_info.so_family = wawo::net::F_AF_INET;

	if (argc == 2) {
		addr_info.so_type = wawo::net::ST_DGRAM;
		addr_info.so_protocol = wawo::net::P_WCP;
	}
	else {
		addr_info.so_type = wawo::net::ST_STREAM;
		addr_info.so_protocol = wawo::net::P_TCP;
	}
	addr_info.so_address = remote_addr;


	WWRP<client_node> node = wawo::make_ref< client_node>(256, addr_info ) ;
	WWRP<client_node::SPT> echo_service = wawo::make_ref<EchoProvider_Client>(services::S_ECHO) ;
	node->add_service( services::S_ECHO, echo_service );

	int start_rt = node->start();
	if (start_rt != wawo::OK)
	{
		return start_rt;
	}
	services::InitSndBytes();

	node->async_make_peer();
	
	application.run_for();
	services::DeinitSndBytes();
	node->stop();

	
	WAWO_WARN("[main]socket server exit done ...");
	return wawo::OK;
}