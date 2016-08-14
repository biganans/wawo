#ifndef _WAWO_NET_REQUEST_HPP
#define _WAWO_NET_REQUEST_HPP


#include <wawo/SmartPtr.hpp>
#include <wawo/algorithm/Packet.hpp>
#include <wawo/net/peer/Http.hpp>

namespace wawo { namespace net {

	using namespace wawo::net::peer;
	using namespace wawo::thread;

	template <class _PeerT, class _MessageT>
	struct WawoMessenger:
		public wawo::net::WawoCallback_Abstract
	{
		enum _State {
			S_IDLE,
			S_SENT,
			S_REQUESTED,
			S_ERROR,
			S_RESPONED
		};

		typedef _PeerT PeerT;
		typedef _MessageT MessageT;

		wawo::thread::Mutex mutex;
		wawo::thread::Condition cond;
		_State state;
		int rcode;

		WWRP<PeerT> peer;
		WWSP<MessageT> message;
		WWSP<wawo::algorithm::Packet> response;

		WawoMessenger(WWRP<PeerT> const& peer, WWSP<MessageT> const& message) :
			peer(peer),
			message(message),
			response(NULL),
			rcode(0),
			state(S_IDLE)
		{
		}

		~WawoMessenger()
		{
		}

		int Request() {
			wawo::thread::UniqueLock<wawo::thread::Mutex> ulk(mutex);
			rcode = peer->Request(message, WWRP<wawo::net::peer::WawoCallback_Abstract>(this));
			if (rcode != wawo::OK)
			{
				state = S_ERROR;
				return rcode;
			}

			state = S_REQUESTED;
			cond.wait(ulk);
			return rcode;
		}

		int Send() {
			wawo::thread::LockGuard<wawo::thread::Mutex> lg(mutex);
			rcode = peer->Send(message);
			if (rcode != wawo::OK) {
				state = S_SENT;
			} else {
				state = S_ERROR;
			}
			return rcode;
		}

		bool OnRespond(WWSP<wawo::algorithm::Packet> const& packet) {
			wawo::thread::LockGuard<wawo::thread::Mutex> lg(mutex);
			WAWO_ASSERT(state == S_REQUESTED);
			response = packet;
			rcode = wawo::OK;
			cond.notify_one();
			return true;
		}
	};


	struct HttpMessenger:
		public HttpCallback_Abstract
	{

		enum _State{
			S_IDLE,
			S_REQUESTED,
			S_ERROR,
			S_RESPONED
		};

	private:
		wawo::thread::Mutex mutex;
		wawo::thread::Condition cond;
		_State state;

		WWRP<Http> httppeer;
		WWSP<message::Http> req;
		WWSP<message::Http> resp;

		int oprtcode;

		inline void _Reset() {
			state = S_IDLE;

			oprtcode = -1;
			httppeer = NULL;
			req = NULL;
			resp = NULL;
		}
	public:

		HttpMessenger() :
			state(S_IDLE),
			httppeer(NULL),
			req(NULL),
			resp(NULL),
			oprtcode(-1)
		{
		}

		~HttpMessenger() {}

		bool OnRespond( WWSP<message::Http> const& response ) {
			LockGuard<Mutex> lg(mutex);
			WAWO_ASSERT(state == S_REQUESTED);
			state = S_RESPONED;

			resp = response;
			oprtcode = wawo::OK;
			cond.notify_one();
			return true;
		}

		virtual bool OnError(int const& ec) {
			LockGuard<Mutex> lg(mutex);
			WAWO_ASSERT(state == S_REQUESTED);
			state = S_ERROR;

			oprtcode = ec;
			cond.notify_one();
			return true;
		}

		int Get(WWRP<Http> const& peer, WWSP<message::Http> const& request, WWSP<message::Http>& response ) {
			UniqueLock<Mutex> ulk(mutex);
			WAWO_ASSERT( state == S_IDLE );
			WAWO_ASSERT(resp == NULL);

			WAWO_ASSERT( (request->GetType() == message::Http::T_NONE) || (request->GetType() == message::Http::T_REQUEST) );
			request->SetType( message::Http::T_REQUEST );

			httppeer = peer;
			req = request;

			oprtcode = peer->Get(request, WWRP<wawo::net::peer::HttpCallback_Abstract>(this));
			WAWO_RETURN_V_IF_NOT_MATCH( oprtcode, oprtcode == wawo::OK );
			state = S_REQUESTED;

			cond.wait(ulk);

			WAWO_ASSERT( resp != NULL );
			WAWO_ASSERT( oprtcode == wawo::OK );
			WAWO_ASSERT( state == S_RESPONED );

			response = resp;
			_Reset();
			return wawo::OK;
		}

		int Post();
		int Head();
		int Put();
		int Delete();
		int Trace();
		int Connect();
	};
}} 

#endif