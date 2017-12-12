#ifndef _WAWO_NET_NODE_ABSTRACT_HPP
#define _WAWO_NET_NODE_ABSTRACT_HPP

#include <wawo/core.hpp>
#include <wawo/net/listener_abstract.hpp>

#define WAWO_LISTEN_BACK_LOG 128


namespace wawo { namespace net {

	template <class _peer_t>
	class node_abstract :
		public listener_abstract<typename _peer_t::peer_event_t>
	{
	public:
		typedef _peer_t peer_t;
		typedef typename _peer_t::peer_event_t peer_event_t;
		typedef typename _peer_t::message_t message_t;

		typedef peer_proxy<_peer_t>				peer_proxy_t;
		typedef listener_abstract<peer_event_t>	peer_event_listener_t;

		typedef node_abstract<peer_t> node_t;

		typedef void(*fn_connect_err_t)(int const& code, WWRP<ref_base> const& cookie);
		typedef std::function<void(int const& code, WWRP<ref_base> const& cookie)> lambda_connect_err_t;

		typedef void(*fn_connect_success_t)(WWRP<peer_t> const& peer, WWRP<socket> const& so, WWRP<ref_base> const& cookie);
		typedef std::function<void(WWRP<peer_t> const& peer, WWRP<socket> const& so, WWRP<ref_base> const& cookie)> lambda_connect_success_t;

		struct async_connect_cookie :
			public ref_base
		{
			WWRP<node_t> node;
			WWRP<socket> so;
			WWRP<ref_base> user_cookie;
			lambda_connect_success_t lambda_connect_success;
			lambda_connect_err_t lambda_connect_err;
		};

		struct async_accept_cookie :
			public ref_base
		{
			WWRP<node_t> node;
		};

	private:
		WWRP<peer_proxy_t> m_peer_proxy;

		typedef std::map<address, WWRP<socket> > listen_socket_pairs;
		typedef std::pair<address, WWRP<socket> > socket_pair;

		spin_mutex m_listen_sockets_mutex;
		listen_socket_pairs m_listen_sockets;

		void _stop_all_listener() {
			std::for_each(m_listen_sockets.begin(), m_listen_sockets.end(), [this](socket_pair const& pair) {
				pair.second->close();
			});
			m_listen_sockets.clear();
		}

	public:
		node_abstract() :
			m_peer_proxy(NULL)
		{
		}

		virtual ~node_abstract() {
			stop();
		}

		int start_listen(wawo::net::socket_addr const& laddr, socket_buffer_cfg const& scfg = socket_buffer_cfgs[BT_MEDIUM]) {

			lock_guard <spin_mutex> lg(m_listen_sockets_mutex);
			WAWO_ASSERT(m_listen_sockets.find(laddr.so_address) == m_listen_sockets.end());

			WWRP<socket> lsocket = wawo::make_ref<socket>(scfg, laddr.so_family, laddr.so_type, laddr.so_protocol);

			int open = lsocket->open();

			if (open != wawo::OK) {
				lsocket->close(open);
				return open;
			}

			if (laddr.so_protocol == P_WCP) {
				int reusert = lsocket->reuse_addr();
				if (reusert != wawo::OK) {
					lsocket->close(reusert);
					return reusert;
				}

				reusert = lsocket->reuse_port();
				if (reusert != wawo::OK) {
					lsocket->close(reusert);
					return reusert;
				}
			}

			int bind = lsocket->bind(laddr.so_address);
			if (bind != wawo::OK) {
				lsocket->close(bind);
				return bind;
			}

			int turn_on_nonblocking = lsocket->turnon_nonblocking();

			if (turn_on_nonblocking != wawo::OK) {
				lsocket->close(turn_on_nonblocking);
				return turn_on_nonblocking;
			}

			int listen_rt = lsocket->listen(WAWO_LISTEN_BACK_LOG);
			if (listen_rt != wawo::OK) {
				lsocket->close(listen_rt);
				return listen_rt;
			}
			WAWO_INFO("[peer_proxy]listen on: %s success, protocol: %s", lsocket->get_local_addr().address_info().cstr, wawo::net::protocol_str[laddr.so_protocol]);

			m_listen_sockets.insert(socket_pair(laddr.so_address, lsocket));

			WWRP<async_accept_cookie> cookie = wawo::make_ref<async_accept_cookie>();
			cookie->node = WWRP<node_t>(this);

			lsocket->begin_async_read(WATCH_OPTION_INFINITE, cookie, node_t::cb_async_accept, node_abstract::cb_async_accept_error);

			return wawo::OK;
		}

		int stop_listen(wawo::net::socket_addr const& addr) {

			lock_guard <spin_mutex> lg(m_listen_sockets_mutex);
			typename listen_socket_pairs::iterator it = m_listen_sockets.find(addr.so_address);
			WAWO_ASSERT(it != m_listen_sockets.end());

			WWRP<socket> so = it->second;
			WAWO_ASSERT(so != NULL);

			m_listen_sockets.erase(it);
			int close_rt = so->close();

			if (close_rt != wawo::OK) {
				WAWO_ERR("[peer_proxy]close listen socket failed: %d", close_rt);
			}

			return close_rt;
		}

		static void cb_async_accept(WWRP<ref_base> const& cookie_) {
			WAWO_ASSERT(cookie_ != NULL);
			WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
			WWRP<async_accept_cookie> user_cookie = wawo::static_pointer_cast<async_accept_cookie>(cookie->user_cookie);
			WAWO_ASSERT(user_cookie->node != NULL);

			WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
			WAWO_ASSERT(cookie->so != NULL);

			user_cookie->node->handle_async_accept(so);
		}

		static void cb_async_accept_error(int const& code, WWRP<ref_base> const& cookie_) {
			WAWO_ASSERT(cookie_ != NULL);
			WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
			WWRP<async_accept_cookie> user_cookie = wawo::static_pointer_cast<async_accept_cookie>(cookie->user_cookie);
			WAWO_ASSERT(user_cookie->node != NULL);

			WWRP<socket> so = wawo::static_pointer_cast<socket>(cookie->so);
			WAWO_ASSERT(cookie->so != NULL);

			user_cookie->node->handle_async_accept_error(code, so);
		}

		void handle_async_accept(WWRP<socket> const& so_) {

			int ec;
			do {
				WWRP<socket> accepted_sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
				u32_t count = so_->accept(accepted_sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec);
				for (u32_t i = 0; i < count; ++i) {
					WWRP<socket>& so = accepted_sockets[i];

					int nonblocking = so->turnon_nonblocking();
					if (nonblocking != wawo::OK) {
						WAWO_ERR("[node_abstract][#%d:%s]turn on nonblocking failed: %d", so->get_fd(), so->get_addr_info().cstr, nonblocking);
						accepted_sockets[i]->close(nonblocking);
						continue;
					}

					int setdefaultkal = so->set_keep_alive_vals(default_keep_alive_vals);
					if (setdefaultkal != wawo::OK) {
						WAWO_ERR("[node_abstract][#%d:%s]set_keep_alive_vals failed: %d", so->get_fd(), so->get_addr_info().cstr, setdefaultkal);
						accepted_sockets[i]->close(nonblocking);
						continue;
					}

					WWRP<peer_t> peer = wawo::make_ref<peer_t>();
					WWRP<tlp_abstract> tlp = wawo::make_ref< typename peer_t::TLPT>();
					so->set_tlp(tlp);

					WWRP<node_t> node(this);
					wawo::task::fn_lambda lambda = [node, peer, so]() -> void {
						node->on_accepted(peer, so );
						so->begin_async_read();
					};

					int addrt = add_peer(peer, so, lambda);
					(void)addrt;

					if (addrt != wawo::OK) {
						WAWO_WARN("[node_abstract][#%d:%s]add peer failed: %d, close socket", so->get_fd(), so->get_local_addr().address_info().cstr, addrt);
						so->close(addrt);
					}
				}
			} while (ec == wawo::E_TRY_AGAIN);

			if (ec != wawo::OK) {
				so_->close(ec);
			}
		}

		void handle_async_accept_error(int const& code, WWRP<socket> const& so_) {
			WAWO_ASSERT(code != wawo::OK);
			WAWO_ASSERT(so_ != NULL);

			WAWO_ERR("[node_abstract][#%d:%s]async accept error: %d", so_->get_fd(), so_->get_addr_info().cstr, code);
			so_->close(code);
		}

		int start() {
			m_peer_proxy = wawo::make_ref<peer_proxy_t>();
			WAWO_CONDITION_CHECK(m_peer_proxy != NULL);

			int rt = m_peer_proxy->start();
			WAWO_CONDITION_CHECK(rt == wawo::OK);
			return rt;
		}

		void stop() {
			if (m_peer_proxy != NULL) {
				m_peer_proxy->stop();
			}

			lock_guard <spin_mutex> lg(m_listen_sockets_mutex);
			_stop_all_listener();
		}

		inline int add_peer(WWRP<peer_t> const& peer, WWRP<socket> const&so, wawo::task::fn_lambda const& lambda = NULL) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			watch_peer_event(E_MESSAGE, peer);
			watch_peer_event(E_RD_SHUTDOWN, peer);
			watch_peer_event(E_WR_SHUTDOWN, peer);
			watch_peer_event(E_CLOSE, peer);
			watch_peer_event(E_WR_BLOCK, peer);
			watch_peer_event(E_WR_UNBLOCK, peer);
			int watchrt = m_peer_proxy->add_peer(peer, so, lambda);

			if (watchrt != wawo::OK) {
				unwatch_peer_event(E_MESSAGE, peer);
				unwatch_peer_event(E_RD_SHUTDOWN, peer);
				unwatch_peer_event(E_WR_SHUTDOWN, peer);
				unwatch_peer_event(E_CLOSE, peer);
				unwatch_peer_event(E_WR_BLOCK, peer);
				unwatch_peer_event(E_WR_UNBLOCK, peer);
			}

			return watchrt;
		}

		inline int remove_peer(WWRP<peer_t> const& peer) {
			WAWO_ASSERT(m_peer_proxy != NULL);
			return m_peer_proxy->remove_peer(peer);
		}

		inline void watch_peer_event(u8_t const& evt_id, WWRP<peer_t> const& peer) {
			peer->register_listener(evt_id, WWRP<peer_event_listener_t>(this));
		}

		inline void unwatch_peer_event(u8_t const& evt_id, WWRP<peer_t> const& peer) {
			peer->unregister_listener(evt_id, WWRP<peer_event_listener_t>(this));
		}

		inline void unwatch_peer_all_event(WWRP<peer_t> const& peer) {
			peer->unregister_listener(WWRP<peer_event_listener_t>(this));
		}

		static void cb_async_connected(WWRP<ref_base> const& cookie) {
			WWRP<async_connect_cookie> _cookie = wawo::static_pointer_cast<async_connect_cookie>(cookie);
			WAWO_ASSERT(_cookie != NULL);

			WAWO_ASSERT(_cookie->so != NULL);
			WAWO_ASSERT(_cookie->node != NULL);
			WAWO_ASSERT(_cookie->user_cookie != NULL);

			WWRP<peer_t> peer = wawo::make_ref<peer_t>();
			wawo::task::fn_lambda lambda = [_cookie, peer ]() -> void {
				_cookie->lambda_connect_success(peer, _cookie->so, _cookie->user_cookie);
				_cookie->so->begin_async_read(WATCH_OPTION_POST_READ_EVENT_AFTER_WATCH);
			};

			int addrt = _cookie->node->add_peer(peer, _cookie->so, lambda);
			if (addrt != wawo::OK) {
				_cookie->so->close(addrt);
				_cookie->lambda_connect_err(addrt, _cookie->user_cookie);
			}
		}

		static void cb_async_connect_error(int const& code, WWRP<ref_base> const& cookie) {
			WAWO_ASSERT(code != wawo::OK);
			WWRP<async_connect_cookie> _cookie = wawo::static_pointer_cast<async_connect_cookie>(cookie);
			WAWO_ASSERT(_cookie != NULL);
			WAWO_ASSERT(_cookie->so != NULL);
			WWRP<socket>& so = _cookie->so;

			WAWO_ASSERT(_cookie->node != NULL);
			WAWO_ASSERT(_cookie->user_cookie != NULL);
			so->close(code);
			_cookie->lambda_connect_err(code, _cookie->user_cookie );
		}

		int connect(socket_addr const& addr, WWRP<socket>& so_o, socket_buffer_cfg const& sbcfg = socket_buffer_cfgs[BT_MEDIUM], keep_alive_vals const& kal = default_keep_alive_vals) {

			WWRP<socket> so = wawo::make_ref<socket>(sbcfg, addr.so_family, addr.so_type, addr.so_protocol);
			WAWO_ALLOC_CHECK(so, sizeof(socket));
			int rt = so->open();

			if (rt != wawo::OK) {
				so->close();
				return rt;
			}

			rt = so->set_keep_alive_vals(kal);

			if (rt != wawo::OK) {
				so->close();
				return rt;
			}

			rt = so->connect(addr.so_address);
			if (rt != wawo::OK) {
				so->close();
				return rt;
			}

			WWRP<tlp_abstract> tlp = wawo::make_ref<typename peer_t::TLPT>();
			WAWO_ALLOC_CHECK(tlp, sizeof(typename peer_t::TLPT));
			so->set_tlp(tlp);
			rt = so->tlp_handshake();

			if (rt != wawo::OK) {
				so->close();
				return rt;
			}

			so_o = so;
			return wawo::OK;
		}
		
		int async_connect(socket_addr const& addr, WWRP<ref_base> const& cookie, fn_connect_success_t const& success, fn_connect_err_t const& err, socket_buffer_cfg const& sbcfg = socket_buffer_cfgs[BT_MEDIUM], keep_alive_vals const& kal = default_keep_alive_vals) {
			lambda_connect_success_t lambda_success = [success](WWRP<peer_t> const& peer, WWRP<socket> const& so, WWRP<ref_base> const& cookie) -> void {
				success(peer, so, cookie);
			};
			lambda_connect_err_t lambda_err = [err](int const& code, WWRP<ref_base> const& cookie) -> void {
				err(code, cookie);
			};
			return _async_connect(addr, cookie, lambda_success, lambda_err, sbcfg, kal);
		}

		int async_connect(socket_addr const& addr, WWRP<ref_base> const& cookie, lambda_connect_success_t const& success, lambda_connect_err_t const& err, socket_buffer_cfg const& sbcfg = socket_buffer_cfgs[BT_MEDIUM], keep_alive_vals const& kal = default_keep_alive_vals) {
			WWRP<async_connect_cookie> _cookie = wawo::make_ref<async_connect_cookie>();
			_cookie->user_cookie = cookie;
			_cookie->node = WWRP<node_t>(this);
			_cookie->lambda_connect_success = success;
			_cookie->lambda_connect_err = err;
			return _async_connect(addr, _cookie, sbcfg, kal);
		}

	private:
		inline int _async_connect(socket_addr const& addr, WWRP<async_connect_cookie> const& cookie, socket_buffer_cfg const& sbcfg = socket_buffer_cfgs[BT_MEDIUM], keep_alive_vals const& kal = default_keep_alive_vals) {
			
			WWRP<socket> so = wawo::make_ref<socket>(sbcfg, addr.so_family, addr.so_type, addr.so_protocol);
			WAWO_ALLOC_CHECK(so, sizeof(socket));
			int rt = so->open();

			if (rt != wawo::OK) {
				so->close(rt);
				return rt;
			}

			rt = so->set_keep_alive_vals(kal);
			if (rt != wawo::OK) {
				so->close(rt);
				return rt;
			}

			WWRP<tlp_abstract> tlp = wawo::make_ref<typename peer_t::TLPT>();
			WAWO_ALLOC_CHECK(tlp, sizeof(typename peer_t::TLPT));
			so->set_tlp(tlp);

			cookie->so = so;
			int connrt = so->async_connect(addr.so_address, cookie, node_t::cb_async_connected, node_t::cb_async_connect_error);

			if (WAWO_LIKELY(connrt == wawo::OK)) {
				return wawo::OK;
			}

			so->close(connrt);
			return connrt;
		}

	public:
		void on_event( WWRP<peer_event_t> const& evt ) {

			WAWO_ASSERT(evt->peer != NULL );
			u32_t const& id = evt->id;

			switch( id ) {
			case wawo::net::E_MESSAGE:
				{
					on_message(evt);
				}
				break;
			case wawo::net::E_RD_SHUTDOWN:
				{
					WAWO_ASSERT(evt->so != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket rd shutdown, %p", evt->so->get_fd(), evt->so->get_addr_info().cstr, evt->peer.get());
					on_socket_read_shutdown(evt);
				}
				break;
			case wawo::net::E_WR_SHUTDOWN:
				{
					WAWO_ASSERT(evt->so != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket wr shutdown, %p", evt->so->get_fd(), evt->so->get_addr_info().cstr, evt->peer.get());
					on_socket_write_shutdown(evt);
				}
				break;
			case wawo::net::E_ERROR:
				{
					WAWO_ASSERT(evt->so != NULL);
					WAWO_DEBUG("[node_abstract][#%d:%s]peer socket error, %p", evt->so->get_fd(), evt->so->get_addr_info().cstr, evt->peer.get() );
					//unwatch_peer_all_event(evt->peer);
					on_error(evt);
				}
				break;
			case wawo::net::E_CLOSE:
				{
					unwatch_peer_all_event(evt->peer);
					on_close(evt);
				}
				break;
			case wawo::net::E_WR_BLOCK:
				{
					on_wr_block(evt);
				}
				break;
			case wawo::net::E_WR_UNBLOCK:
				{
					on_wr_unblock(evt);
				}
				break;
			default:
				{
					WAWO_THROW("unknown evt");
				}
				break;
			}
		}

		virtual void on_socket_read_shutdown(WWRP<peer_event_t> const& evt) {
			evt->so->close(evt->info.int32_v);
		}
		virtual void on_socket_write_shutdown(WWRP<peer_event_t> const& evt) {
			evt->so->close(evt->info.int32_v);
		}
		virtual void on_error(WWRP<peer_event_t> const& evt) {
			evt->so->close(evt->info.int32_v);
		};

		virtual void on_wr_block(WWRP<peer_event_t> const& evt) { (void)evt; }
		virtual void on_wr_unblock(WWRP<peer_event_t> const& evt) { (void)evt; }

		virtual void on_close(WWRP<peer_event_t> const& evt) { (void)evt; };

		virtual void on_accepted(WWRP<peer_t> const& peer, WWRP<socket> const& so ) {
			(void)peer; (void)so;

			char _tmp[1024] = {0};
			snprintf(_tmp, 1024, "[node_abstract][%d:%s]local addr: %s, on accepted, you must override this method for your own", so->get_fd(), so->get_addr_info().cstr , so->get_local_addr().address_info().cstr );

			WAWO_THROW(_tmp);
		}

		virtual void on_message(WWRP<peer_event_t> const& evt) = 0;
	};

}}
#endif
