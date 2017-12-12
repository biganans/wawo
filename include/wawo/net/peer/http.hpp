#ifndef WAWO_NET_PEER_HTTP_HPP_
#define WAWO_NET_PEER_HTTP_HPP_

#include <wawo/core.hpp>
#include <wawo/thread/mutex.hpp>

#include <wawo/net/listener_abstract.hpp>
#include <wawo/net/dispatcher_abstract.hpp>
#include <wawo/net/tlp/stream.hpp>

#include <wawo/net/net_event.hpp>
#include <wawo/net/peer_abstract.hpp>
#include <wawo/net/peer/message/http.hpp>
#include <wawo/net/socket.hpp>

#include <wawo/net/protocol/http.hpp>

struct http_parser;

namespace wawo { namespace net { namespace peer {

	using namespace wawo::thread;

	struct http_callback_abstract :
		public wawo::ref_base
	{
		virtual bool on_respond(WWSP<message::http> const& response) = 0 ;
		virtual bool on_error(int const& ec) = 0;
	};

	class http:
		public peer_abstract<>,
		public dispatcher_abstract< peer_event<http> >
	{

	public:
		typedef tlp::stream TLPT;
		typedef http self_t;
		typedef message::http message_t;
		typedef peer_abstract<> peer_abstract_t;
		typedef typename peer_abstract_t::socket_event_listener_t socket_event_listener_t;
		typedef dispatcher_abstract< peer_event<http> > dispatcher_t;
		typedef peer_event<http> peer_event_t;

	private:
		struct requested_message
		{
			WWSP<message::http> message;
			WWRP<wawo::net::peer::http_callback_abstract> cb;
			u64_t ts_request;
			u32_t timeout;
		};
		typedef std::queue<requested_message> RequestedMessageQueue ;

	private:
		http_parser* m_hparser;

		spin_mutex m_requested_mutex;
		RequestedMessageQueue m_requested;

		WWSP<message::http> m_tmp;
		len_cstr m_tmp_field;
		len_cstr m_tmp_body;

		void _alloc_http_parser();
		void _dealloc_http_parser();

	public:
		http():
			m_hparser(NULL)
		{
		}

		~http()
		{
			_dealloc_http_parser();
		}

		//BLOCK CONNECT
		int connect(wawo::len_cstr const& hosturl);
		
		virtual void attach_socket( WWRP<socket> const& so ) {
			lock_guard<shared_mutex> lg(m_mutex);
			_register_evts(so);
			m_so = so;
			_alloc_http_parser();
		}

		int http_parser_on_message_begin() {
			m_tmp = wawo::make_shared<message::http>();
			m_tmp_field = "";
			m_tmp_body = "";
			return wawo::OK;
		}

		int http_parser_on_message_complete();

		int http_parser_on_header_field( const char* data, size_t len) {
			WAWO_ASSERT( m_tmp != NULL );
			m_tmp_field = len_cstr(data, len);
			return 0;
		}
		int http_parser_on_header_value(const char* data, size_t len) {
			WAWO_ASSERT( m_tmp_field.len > 0 );
			m_tmp->add_header_line( m_tmp_field, len_cstr(data,len) );
			return 0;
		}
		int http_parser_on_header_complete() {
			return 0;
		}
		int http_parser_on_body(const char* data, size_t len) {
			m_tmp_body += len_cstr(data, len);
			return 0;
		}
		int http_parser_on_url(const char* data, size_t len) {
			m_tmp->url = len_cstr(data, len) ;
			return 0;
		}
		int http_parser_on_status(const char* data, size_t len) {
			m_tmp->status = len_cstr(data, len);
			return 0;
		}

		int http_parser_on_chunk_header() { return 0; }
		int http_parser_on_chunk_complete() { return 0; }

		void on_event(WWRP<socket_event> const& evt);

		int do_send_message(WWSP<message::http> const& message) {
			shared_lock_guard<shared_mutex> slg(m_mutex);
			if (m_so == NULL) {
				return wawo::E_PEER_NO_SOCKET_ATTACHED;
			}

			WWSP<packet> httppack;
			int mencrt = message->encode(httppack);
			WAWO_RETURN_V_IF_NOT_MATCH(mencrt, (mencrt == wawo::OK));
			return m_so->send_packet(httppack);
		}

		int respond(WWSP<message::http> const& resp, WWSP<message::http> const& req) {
			WAWO_ASSERT(req->type == message::http::T_REQUEST);
			WAWO_ASSERT(req->option != protocol::http::O_NONE);

			WAWO_ASSERT(resp->type == message::http::T_RESPONSE || resp->type == message::http::T_NONE );
			WAWO_ASSERT(resp->option == protocol::http::O_NONE);

			resp->type = message::http::T_RESPONSE ;
			return self_t::do_send_message(resp);
		}

		int get(WWSP<message::http> const& message, WWRP<wawo::net::peer::http_callback_abstract> const& cb, u32_t const& timeout = 30);
		int post(WWSP<message::http> const& message, WWRP<wawo::net::peer::http_callback_abstract> const& cb, u32_t const& timeout = 30);

		/*
			@TODO
			the following api TBD
		*/
		int Head();
		int Put();
		int Delete();
		int Trace();
		int connect();
	};
}}}
#endif
