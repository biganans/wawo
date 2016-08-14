#include <wawo.h>
#include "services/ServiceCommand.h"

typedef wawo::net::peer::Wawo<> PeerT;
typedef typename PeerT::MessageT MessageT;


int main( int argc, char** argv ) {

	wawo::app::App application ;

	wawo::net::SocketAddr remote_addr( "192.168.2.7", 12121 );
	WAWO_REF_PTR<wawo::net::Socket> socket ( new wawo::net::Socket( remote_addr,wawo::net::F_AF_INET , wawo::net::ST_STREAM, wawo::net::P_TCP ) );
	int rt = socket->Open();
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
	rt = socket->Connect();
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	WWRP<wawo::net::core::TLP_Abstract> tlp(new typename PeerT::TLPT());
	socket->SetTLP(tlp);
	rt = socket->TLP_Handshake();
	WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);

	WAWO_REF_PTR<PeerT> peer(new PeerT());
	peer->AttachSocket(socket);

	int sndhellort;
	PEER_SND_HELLO(peer, sndhellort);
	WAWO_ASSERT(sndhellort==wawo::OK);

	int ec;
	do {
		WWSP<MessageT> arrives[5];
		wawo::u32_t count = peer->DoReceiveMessages(arrives, 5, ec);

		WAWO_ASSERT(count == 1);
		WAWO_ASSERT(ec == wawo::OK);
		WAWO_ASSERT(arrives[0]->GetType() == wawo::net::peer::message::Wawo::T_RESPONSE );

		wawo::Len_CStr reqstr("this is request from client");
		WWSP<wawo::algorithm::Packet> reqpack( new wawo::algorithm::Packet() );

		reqpack->Write<wawo::u32_t>(services::C_ECHO_STRING_REQUEST_TEST);
		reqpack->Write<wawo::u32_t>(reqstr.Len());
		reqpack->Write((wawo::byte_t*)reqstr.CStr(),reqstr.Len());

		reqpack->WriteLeft < wawo::net::ServiceIdT >(services::S_ECHO);
		WWSP< MessageT> message_to_req(new MessageT(reqpack));
		int sndrt = peer->Request(message_to_req);
		WAWO_ASSERT(sndrt == wawo::OK);
	} while (ec == wawo::OK);

	WAWO_LOG_WARN( "main", "socket server exit ..." ) ;
	return wawo::OK;
}