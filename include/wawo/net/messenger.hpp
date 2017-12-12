#ifndef _WAWO_NET_REQUEST_HPP
#define _WAWO_NET_REQUEST_HPP


#include <wawo/smart_ptr.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/peer/http.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::peer;
	using namespace wawo::thread;

	template <class _peer_t, class _MessageT>
	struct ros_messenger:
		public wawo::net::ros_callback_abstract
	{
		enum _State {
			S_IDLE,
			S_SENT,
			S_REQUESTED,
			S_ERROR,
			S_RESPONED
		};

		typedef _peer_t peer_t;
		typedef _MessageT message_t;

		wawo::thread::mutex mutex;
		wawo::thread::condition cond;
		_State state;
		int rcode;

		WWRP<peer_t> peer;
		WWSP<message_t> message;
		WWSP<wawo::packet> response;

		ros_messenger(WWRP<peer_t> const& peer, WWSP<message_t> const& message) :
			peer(peer),
			message(message),
			response(NULL),
			rcode(0),
			state(S_IDLE)
		{
		}

		~ros_messenger()
		{
		}

		int request() {
			wawo::thread::unique_lock<wawo::thread::mutex> ulk(mutex);
			rcode = peer->request(message, WWRP<wawo::net::peer::ros_callback_abstract>(this));
			if (rcode != wawo::OK)
			{
				state = S_ERROR;
				return rcode;
			}

			state = S_REQUESTED;
			cond.wait(ulk);
			return rcode;
		}

		int send() {
			wawo::thread::lock_guard<wawo::thread::mutex> lg(mutex);
			rcode = peer->send(message);
			if (rcode != wawo::OK) {
				state = S_SENT;
			} else {
				state = S_ERROR;
			}
			return rcode;
		}

		bool on_respond(WWSP<wawo::packet> const& packet) {
			wawo::thread::lock_guard<wawo::thread::mutex> lg(mutex);
			WAWO_ASSERT(state == S_REQUESTED);
			response = packet;
			rcode = wawo::OK;
			cond.notify_one();
			return true;
		}
	};


	struct http_messenger:
		public http_callback_abstract
	{

		enum _State{
			S_IDLE,
			S_REQUESTED,
			S_ERROR,
			S_RESPONED
		};

	private:
		wawo::thread::mutex mtx;
		wawo::thread::condition cond;
		_State state;

		WWRP<http> httppeer;
		WWSP<wawo::net::peer::message::http> req;
		WWSP<wawo::net::peer::message::http> resp;

		int oprtcode;

		inline void _reset() {
			state = S_IDLE;

			oprtcode = -1;
			httppeer = NULL;
			req = NULL;
			resp = NULL;
		}
	public:

		http_messenger() :
			state(S_IDLE),
			httppeer(NULL),
			req(NULL),
			resp(NULL),
			oprtcode(-1)
		{
		}

		~http_messenger() {}

		bool on_respond( WWSP<wawo::net::peer::message::http> const& response ) {
			lock_guard<mutex> lg(mtx);
			WAWO_ASSERT(state == S_REQUESTED);
			state = S_RESPONED;

			resp = response;
			oprtcode = wawo::OK;
			cond.notify_one();
			return true;
		}

		virtual bool on_error(int const& ec) {
			lock_guard<mutex> lg(mtx);
			WAWO_ASSERT(state == S_REQUESTED);
			state = S_ERROR;

			oprtcode = ec;
			cond.notify_one();
			return true;
		}

		int get(WWRP<http> const& peer, WWSP<message::http> const& request, WWSP<message::http>& response ) {
			unique_lock<mutex> ulk(mtx);
			WAWO_ASSERT( state == S_IDLE );
			WAWO_ASSERT(resp == NULL);

			WAWO_ASSERT( (request->type == message::http::T_NONE) || (request->type == message::http::T_REQUEST) );
			request->type = message::http::T_REQUEST ;

			httppeer = peer;
			req = request;

			oprtcode = peer->get(request, WWRP<wawo::net::peer::http_callback_abstract>(this));
			WAWO_RETURN_V_IF_NOT_MATCH( oprtcode, oprtcode == wawo::OK );
			state = S_REQUESTED;

			cond.wait(ulk);

			WAWO_ASSERT( resp != NULL );
			WAWO_ASSERT( oprtcode == wawo::OK );
			WAWO_ASSERT( state == S_RESPONED );

			response = resp;
			_reset();
			return wawo::OK;
		}

		int post();
		int Head();
		int Put();
		int Delete();
		int Trace();
		int connect();
	};
}} 

#endif