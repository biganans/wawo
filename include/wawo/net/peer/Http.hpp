#ifndef WAWO_NET_PEER_HTTP_HPP_
#define WAWO_NET_PEER_HTTP_HPP_

#include <vector>

#include <wawo/core.h>
#include <wawo/thread/Mutex.hpp>

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/core/Dispatcher_Abstract.hpp>
#include <wawo/net/core/TLP/Stream.hpp>

#include <wawo/net/NetEvent.hpp>
#include <wawo/net/Peer_Abstract.hpp>
#include <wawo/net/peer/message/Http.hpp>
#include <wawo/net/Socket.hpp>

struct http_parser;


namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;
	using namespace wawo::net::core;

	struct HttpCallback_Abstract :
		public wawo::RefObject_Abstract
	{
		virtual bool OnRespond(WWSP<message::Http> const& response) = 0 ;
		virtual bool OnError(int const& ec) = 0;
	};

	class Http:
		public Peer_Abstract,
		public core::Listener_Abstract<SocketEvent>,
		public core::Dispatcher_Abstract< PeerEvent<Http> >
	{

	public:
		typedef TLP::Stream TLPT;
		typedef Http MyT;
		typedef message::Http MessageT;
		typedef core::Listener_Abstract<SocketEvent> ListenerT;
		typedef core::Dispatcher_Abstract< PeerEvent<Http> > DispatcherT;
		typedef PeerEvent<Http> PeerEventT;

	private:
		struct RequestedMessage
		{
			WWSP<message::Http> message;
			WWRP<wawo::net::peer::HttpCallback_Abstract> cb;
			u64_t ts_request;
			u32_t timeout;
		};
		typedef std::queue<RequestedMessage> RequestedMessageQueue ;

	private:
		SharedMutex m_mutex;
		WWRP<Socket> m_socket;
		http_parser* m_hparser;

		SpinMutex m_requested_mutex;
		RequestedMessageQueue m_requested;

		WWSP<message::Http> m_tmp;
		Len_CStr m_tmp_field;
		Len_CStr m_tmp_body;

		void _AllocHttpParser();
		void _DeallocHttpParser();

	public:
		Http():
			m_hparser(NULL)
		{
		}

		~Http()
		{
			_DeallocHttpParser();
		}

		//BLOCK CONNECT
		int Connect(wawo::Len_CStr const& hosturl);

		void AttachSocket( WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			WAWO_ASSERT( m_socket == NULL);
			WAWO_ASSERT( socket != NULL );
			m_socket = socket;

			WWRP<ListenerT> peer_l( this );
			socket->Register(SE_PACKET_ARRIVE, peer_l);
			socket->Register(SE_CONNECTED, peer_l);
			socket->Register(SE_CLOSE, peer_l);
			socket->Register(SE_RD_SHUTDOWN, peer_l);
			socket->Register(SE_WR_SHUTDOWN, peer_l);
			socket->Register(SE_ERROR, peer_l);

			_AllocHttpParser();
		}

		void DetachSocket( WWRP<Socket> const& socket ) {
			LockGuard<SharedMutex> lg(m_mutex);
			if( m_socket != NULL ) {
				WAWO_ASSERT( m_socket == socket );
				WWRP<ListenerT> peer_l( this );
				m_socket->UnRegister( peer_l );
				m_socket = NULL;
			}
		}


		void GetSockets( std::vector< WWRP<Socket> >& sockets ) {
			SharedLockGuard<SharedMutex> slg( m_mutex );
			if( m_socket != NULL ) {
				sockets.push_back(m_socket);
			}
		}

		bool HaveSocket(WWRP<Socket> const& socket) {
			SharedLockGuard<SharedMutex> slg( m_mutex );
			return m_socket == socket;
		}

		int Close( int const& c = 0) {
			SharedLockGuard<SharedMutex> slg( m_mutex );
			if( m_socket != NULL ) {
				return m_socket->Close(c);
			}
			return wawo::OK;
		}

		int OnMessageBegin() {
			m_tmp = WWSP<message::Http>( new message::Http() );
			m_tmp_field = "";
			m_tmp_body = "";
			return wawo::OK;
		}

		int OnMessageComplete();

		int OnHeaderField( const char* data, size_t length) {
			WAWO_ASSERT( m_tmp != NULL );
			m_tmp_field = Len_CStr(data, length);
			return 0;
		}
		int OnHeaderValue(const char* data, size_t length) {
			WAWO_ASSERT( m_tmp_field.Len() > 0 );
			m_tmp->AddHeader( m_tmp_field, Len_CStr(data,length) );
			return 0;
		}
		int OnHeaderComplete() {
			return 0;
		}
		int OnBody(const char* data, size_t length) {
			m_tmp_body += Len_CStr(data, length);
			return 0;
		}
		int OnUrl(const char* data, size_t length) {
			m_tmp->SetUrl( Len_CStr(data, length) );
			return 0;
		}
		int OnStatus(const char* data, size_t length) {
			m_tmp->SetStatus(Len_CStr(data, length));
			return 0;
		}

		int OnChunkHeader() { return 0; }
		int OnChunkComplete() { return 0; }

		void OnEvent(WWRP<SocketEvent> const& evt);

		int DoSendMessage(WWSP<message::Http> const& message) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if (m_socket == NULL) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<Packet> httppack;
			int mencrt = message->Encode(httppack);
			WAWO_RETURN_V_IF_NOT_MATCH(mencrt, (mencrt == wawo::OK));
			return m_socket->SendPacket(httppack);
		}

		int Respond(WWSP<message::Http> const& resp, WWSP<message::Http> const& req) {
			WAWO_ASSERT(req->GetType() == message::Http::T_REQUEST);
			WAWO_ASSERT(req->GetOption() != http::O_NONE);

			WAWO_ASSERT(resp->GetType() == message::Http::T_RESPONSE || resp->GetType() == message::Http::T_NONE );
			WAWO_ASSERT(resp->GetOption() == http::O_NONE);

			resp->SetType(message::Http::T_RESPONSE);
			return MyT::DoSendMessage(resp);
		}

		int Get(WWSP<message::Http> const& message, WWRP<wawo::net::peer::HttpCallback_Abstract> const& cb, u32_t const& timeout = 30);
		int Post(WWSP<message::Http> const& message, WWRP<wawo::net::peer::HttpCallback_Abstract> const& cb, u32_t const& timeout = 30);

		/*
			@TODO
			the following api TBD
		*/
		int Head();
		int Put();
		int Delete();
		int Trace();
		int Connect();
	};
}}}
#endif
