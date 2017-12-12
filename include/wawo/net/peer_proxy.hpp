#ifndef _WAWO_NET_PEER_PROXY_HPP_
#define _WAWO_NET_PEER_PROXY_HPP_

#include <map>

#include <wawo/core.hpp>
#include <wawo/net/net_event.hpp>
#include <wawo/net/listener_abstract.hpp>
#include <wawo/thread/ticker.hpp>
#include <wawo/singleton.hpp>


/*
* Peer proxy is used to manage peers that accepted from remote connect , and the peers that we connect to remote actively.
*/

namespace wawo { namespace net {

		typedef milli_ticker peer_proxy_ticker;

		using namespace wawo::thread;

		template <class _peer_t = wawo::net::peer::ros<> >
		class peer_proxy :
			public listener_abstract< peer_event<_peer_t> >
		{
			const wawo::u64_t PEER_TICKER_FREQ ;// = _peer_t::PEER_TICKER_FREQ ;
		public:
			typedef _peer_t peer_t;
			typedef peer_proxy<_peer_t> self_t;
			typedef peer_event<_peer_t> peer_event_t;

			typedef listener_abstract< peer_event<_peer_t> > peer_event_listener_t;

		private:
			class peer_ticker :
				public wawo::thread::ticker<_peer_t::PEER_TICKER_FREQ, 1>,
				public wawo::singleton< peer_ticker>
			{};

			typedef typename std::conditional< (_peer_t::PEER_TICKER_FREQ >= peer_proxy_ticker::PRECISION_INTERVAL), peer_proxy_ticker, peer_ticker >::type peer_ticker_t;
			typedef std::vector< WWRP<peer_t> > peer_vector;
			typedef std::vector< WWRP<socket>> socket_vector;

			enum _RunState {
				S_IDLE,
				S_RUN,
				S_EXIT
			};

			enum peer_op_code {
				OP_NONE,
				OP_PEER_ADD,
				OP_PEER_REMOVE,

				OP_ASYNC_WRITE_BEGIN ,
				OP_ASYNC_WRITE_END
			};

			struct peer_op {
				peer_op_code code;
				WWRP<peer_t> peer;
				WWRP<socket> so;
			};

			typedef std::queue<peer_op> PeerOpQueue;

		private:
			shared_mutex m_mutex;
			int m_state;

			spin_mutex m_ops_mutex;
			PeerOpQueue m_ops;

			spin_mutex m_peers_mutex;
			peer_vector m_peers;

			socket_vector m_async_write_sockets;
			u64_t m_last_socket_async_send_check_timer;

			WWSP<wawo::thread::fn_ticker> m_peer_ticker_fn;
			WWSP<wawo::thread::fn_ticker> m_ticker_fn;

		private:
			inline void _plan_op(peer_op const& op) {
				lock_guard<spin_mutex> lg_ops(m_ops_mutex);
				m_ops.push(op);
			}
			inline void _pop_op(peer_op& op) {
				lock_guard<spin_mutex> lg_ops(m_ops_mutex);
				if (m_ops.empty()) return;
				op = m_ops.front();
				m_ops.pop();
			}

			inline void _execute_ops() {

				while (!m_ops.empty()) {
					peer_op pop;
					_pop_op(pop);
					if (pop.code == OP_NONE) {
						break;
					}

					WWRP<peer_t> const& peer = pop.peer;
					WWRP<socket> const& so = pop.so;
					peer_op_code const& code = pop.code;

					(void)so;
					switch (code) {
					case OP_PEER_ADD:
					{
						_add_peer(peer);
					}
					break;
					case OP_PEER_REMOVE:
					{
						peer->detach_socket();
						_remove_peer(peer);
					}
					break;
					case OP_ASYNC_WRITE_BEGIN:
					{
						socket_vector::iterator it = std::find(m_async_write_sockets.begin(), m_async_write_sockets.end(), so);
						if (it == m_async_write_sockets.end()) {
							m_async_write_sockets.push_back(so);
						}
					}
					break;
					case OP_ASYNC_WRITE_END:
					{
						socket_vector::iterator it = std::find(m_async_write_sockets.begin(), m_async_write_sockets.end(), so);
						if (it != m_async_write_sockets.end()) {
							m_async_write_sockets.erase(it);
						}
					}
					break;
					case OP_NONE:
					{//just for compile warning
					}
					break;
					default:
					{
						char tmp[256] = { 0 };
						snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown peer opcode : %d", code);
						WAWO_THROW(tmp);
					}
					break;
					}
				}
			}

			inline void _check_async_write_sockets(u64_t const& now) {

				if (now < (m_last_socket_async_send_check_timer + 1000 * 10)) {
					return;
				}

				socket_vector::iterator it = m_async_write_sockets.begin();
				while (it != m_async_write_sockets.end()) {
					if ((*it)->is_write_shutdowned()) {
						it = m_async_write_sockets.erase(it);
						continue;
					}

					if ((*it)->is_flush_timer_expired(now)) {
						(*it)->shutdown(wawo::net::SHUTDOWN_RDWR, wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED);
						it = m_async_write_sockets.erase(it);
					}
					else {
						++it;
					}
				}
			}

			int _init() {
				{
					lock_guard<shared_mutex> _lg(m_mutex);
					WAWO_ASSERT(m_state == S_IDLE);
					m_state = S_RUN;

					m_ticker_fn = wawo::make_shared<wawo::thread::fn_ticker>(std::bind(&self_t::update, WWRP<self_t>(this)));

					if (wawo::is_true< (_peer_t::PEER_TICKER_FREQ > 0) > ()) {
						m_peer_ticker_fn = wawo::make_shared<wawo::thread::fn_ticker>(std::bind(&self_t::_peer_ticker, WWRP<self_t>(this)));
					}
				}

				peer_proxy_ticker::instance()->schedule(m_ticker_fn);
				if (wawo::is_true< (_peer_t::PEER_TICKER_FREQ>0) > ()) {
					peer_ticker_t::instance()->schedule(m_peer_ticker_fn, PEER_TICKER_FREQ);
				}
				return wawo::OK;
			}

			void _deinit() {

				{
					lock_guard<shared_mutex> _lg(m_mutex);
					WAWO_ASSERT(m_state == S_EXIT);

					WAWO_ASSERT(m_ticker_fn != NULL);
					_execute_ops();

					lock_guard<spin_mutex> lg_peers(m_peers_mutex);
					std::for_each(m_peers.begin(), m_peers.end(), [this](WWRP<peer_t> const& peer) {
						WAWO_ASSERT(peer->get_socket() != NULL);

						WWRP<socket> so = peer->get_socket();
						peer->unregister_listener(WWRP<peer_event_listener_t>(this));
						so->close(wawo::E_PEER_PROXY_EXIT);
						peer->detach_socket();

						//last time to try to deliver CLOSE
						WWRP<peer_event_t> _evt = wawo::make_ref<peer_event_t>(E_CLOSE, peer, so);
						peer->oschedule(_evt, so->get_dp_id());
					});
					m_peers.clear();

					WAWO_ASSERT(m_peers.size() == 0);
					lock_guard<spin_mutex> lg(m_ops_mutex);
					WAWO_ASSERT(m_ops.empty());
				}
				if (m_ticker_fn != NULL) {
					peer_proxy_ticker::instance()->deschedule(m_ticker_fn);
				}

				if (m_peer_ticker_fn != NULL) {
					peer_ticker_t::instance()->deschedule(m_peer_ticker_fn);
				}
				m_ticker_fn = NULL;
				m_peer_ticker_fn = NULL;
			}

			void _peer_ticker() {
				lock_guard<spin_mutex> peers_lg(m_peers_mutex);
				if (m_state == S_RUN) {
					u32_t c = m_peers.size();
					u64_t now = wawo::time::curr_microseconds();
					for (u32_t i = 0; i < c; ++i) {
						m_peers[i]->tick(now);
					}
				}
			}

		public:
			peer_proxy() :
				PEER_TICKER_FREQ(_peer_t::PEER_TICKER_FREQ),
				m_state(S_IDLE),
				m_last_socket_async_send_check_timer(0)
			{
			}

			virtual ~peer_proxy() { WAWO_ASSERT(m_state == S_EXIT); }

			int start() {
				return _init();
			}

			//stop must be called explicitily
			void stop() {
				{
					lock_guard<shared_mutex> _lg(m_mutex);
					if (m_state == S_EXIT) { return; }
					m_state = S_EXIT;
				}
				_deinit();
			}

			void update() {
				shared_lock_guard<shared_mutex> lg(m_mutex);
				if(m_state == S_RUN)
				{
					u64_t now = wawo::time::curr_milliseconds();
					_execute_ops();
					_check_async_write_sockets(now);
				}
			}

			int add_peer(WWRP<peer_t> const& peer, WWRP<socket> const& so, wawo::task::fn_lambda const& schedule_if_add_ok ) {
				WAWO_ASSERT(peer != NULL);
				WAWO_ASSERT(so != NULL);
				shared_lock_guard<shared_mutex> lg_mutex(m_mutex);
				if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }
				peer->attach_socket(so);
				_add_peer(peer);
				if (schedule_if_add_ok != NULL) {
					peer->oschedule(schedule_if_add_ok);
				}
				return wawo::OK;
			}

			inline void _add_peer(WWRP<peer_t> const& peer) {
				WAWO_ASSERT(peer != NULL);
				WAWO_ASSERT(m_state == S_RUN);
				WAWO_ASSERT(peer->get_socket() != NULL);

				WWRP<peer_event_listener_t> peer_l(this);
				peer->register_listener(E_CLOSE, peer_l);
				peer->register_listener(E_WR_BLOCK, peer_l);
				peer->register_listener(E_WR_UNBLOCK, peer_l);
				lock_guard<spin_mutex> lg(m_peers_mutex);
				typename peer_vector::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
				WAWO_ASSERT(it == m_peers.end());
				m_peers.push_back(peer);
			}

			int remove_peer(WWRP<peer_t> const& peer) {
				WAWO_ASSERT(peer != NULL);

				shared_lock_guard<shared_mutex> lg_mutex(m_mutex);
				if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }
				peer->detach_socket();
				_remove_peer(peer);
				return wawo::OK;
			}

			inline void _remove_peer(WWRP<peer_t> const& peer) {
				WAWO_ASSERT(peer != NULL);

				WWRP<peer_event_listener_t> peer_l(this);
				peer->unregister_listener( peer_l);

				lock_guard<spin_mutex> lg(m_peers_mutex);
				typename peer_vector::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
				WAWO_ASSERT(it != m_peers.end());
				m_peers.erase(it);
			}

			void on_event(WWRP<peer_event_t> const& evt) {
				shared_lock_guard<shared_mutex> slg(m_mutex);
				if (m_state != S_RUN) {
					//evt->peer->detach_socket();
					evt->so->close();
					evt->peer->unregister_listener( WWRP<peer_event_listener_t>(this) );
					return;
				}

				switch (evt->id) {
				case E_CLOSE:
				{
					peer_op op = { OP_PEER_REMOVE, evt->peer, evt->so };
					_plan_op(op);
				}
				break;
				case E_WR_BLOCK:
				{
					WAWO_ASSERT(evt->so->is_data_socket());
					WAWO_ASSERT(evt->so->is_nonblocking());

					peer_op op = { OP_ASYNC_WRITE_BEGIN, evt->peer, evt->so };
					_plan_op(op);
				}
				break;
				case E_WR_UNBLOCK:
				{
					WAWO_ASSERT(evt->so->is_data_socket());
					WAWO_ASSERT(evt->so->is_nonblocking());
					WAWO_ASSERT(evt->info.int32_v == 0);
					peer_op op = { OP_ASYNC_WRITE_END, evt->peer, evt->so };
					_plan_op(op);
				}
				break;
				default:
				{
					char _tbuffer[256] = { 0 };
					snprintf(_tbuffer, sizeof(_tbuffer) / sizeof(_tbuffer[0]), "[#%d:%s]invalid peer event, eid: %d", evt->so->get_fd(), evt->so->get_addr_info().cstr, evt->id );
					WAWO_THROW(_tbuffer);
				}
				break;
				}
			}
		};
	}
}
#endif