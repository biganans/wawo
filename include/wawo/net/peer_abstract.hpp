#ifndef _WAWO_NET_PEER_ABSTRACT_HPP
#define _WAWO_NET_PEER_ABSTRACT_HPP

#include <vector>
#include <wawo/smart_ptr.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	template <u64_t _ticker_freq=0>
	class peer_abstract:
		public listener_abstract< socket_event >
	{

	public:
		typedef listener_abstract< socket_event > socket_event_listener_t;

	public:
		const static u64_t PEER_TICKER_FREQ=_ticker_freq;

	protected:
		shared_mutex m_mutex;
		WWRP<socket> m_so;
		WWRP<ref_base> m_ctx;

	public:
		explicit peer_abstract():
			m_ctx(NULL)
		{
		}

		/*
		* u must detach all your sockets before peer's destruction call
		*/
		virtual ~peer_abstract() {}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			lock_guard<shared_mutex> _lg( *(const_cast<shared_mutex*>(&m_mutex)));
			return wawo::static_pointer_cast<ctx_t>(m_ctx);;
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			lock_guard<shared_mutex> _lg(m_mutex);
			m_ctx = ctx;
		}

		inline void _register_evts(WWRP<socket> const& so) {
			WAWO_ASSERT(m_so == NULL);
			WAWO_ASSERT(so != NULL);

			WWRP<socket_event_listener_t> peer_l(this);
			so->register_listener(E_PACKET_ARRIVE, peer_l);
			so->register_listener(E_RD_SHUTDOWN, peer_l);
			so->register_listener(E_WR_SHUTDOWN, peer_l);
			so->register_listener(E_CLOSE, peer_l);
			so->register_listener(E_WR_BLOCK, peer_l);
			so->register_listener(E_WR_UNBLOCK, peer_l);
			so->register_listener(E_ERROR, peer_l);
		}

		inline void _unregister_evts(WWRP<socket> const& so) {
			WWRP<socket_event_listener_t> peer_l(this);
			so->unregister_listener(peer_l);
		}

		virtual void attach_socket(WWRP<socket> const& so) {
			lock_guard<shared_mutex> lg(m_mutex);
			_register_evts(so);
			m_so = so;
		}
		virtual void detach_socket() {
			lock_guard<shared_mutex> lg(m_mutex);
			if (m_so != NULL) {
				_unregister_evts(m_so);
				m_so = NULL;
			}
		}

		int close( int const& code=0 ) {
			shared_lock_guard<shared_mutex> lg(m_mutex);
			if (m_so != NULL) {
				return m_so->close(code);
			}
			return wawo::OK;
		}

		int shutdown(u8_t const& flag, int const& ec=0) {
			shared_lock_guard<shared_mutex> lg(m_mutex);
			if (m_so != NULL) {
				return m_so->shutdown(flag, ec);
			}

			return wawo::OK;
		}

		inline WWRP<socket> get_socket() {
			shared_lock_guard<shared_mutex> slg(m_mutex);
			return m_so;
		}

		virtual void tick(u64_t const& now) { WAWO_ASSERT(PEER_TICKER_FREQ != 0); (void)now; }
		virtual void on_event(WWRP<socket_event> const& evt) = 0 ;
	};
}}
#endif