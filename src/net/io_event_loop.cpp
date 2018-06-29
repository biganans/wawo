#include <wawo/core.hpp>
#ifdef WAWO_ENABLE_EPOLL
#include <wawo/net/poller_impl/epoll.hpp>
#elif defined(WAWO_ENABLE_IOCP)
#include <wawo/net/poller_impl/iocp.hpp>
#else
#include <wawo/net/poller_impl/select.hpp>
#endif

#ifdef WAWO_ENABLE_WCP
#include <wawo/net/poller_impl/wpoll.hpp>
#include <wawo/net/wcp.hpp>
#endif

#include <wawo/net/io_event_loop.hpp>

namespace wawo { namespace net {

	inline static WWRP<io_event_loop> make_poller_by_type(poller_type t) {
			WWRP<io_event_loop> rt;
			switch (t) {
#ifdef WAWO_ENABLE_EPOLL
			case T_EPOLL:
			{
				rt = wawo::make_ref<impl::epoll>();
				WAWO_ALLOC_CHECK(m_poller, sizeof(impl::epoll));
			}
			break;

#elif defined(WAWO_ENABLE_IOCP)

			case T_IOCP:
			{
				rt = wawo::make_ref<impl::iocp>();
				WAWO_ALLOC_CHECK(rt, sizeof(impl::iocp));
			}
			break;
#else
			case T_SELECT:
			{
				rt = wawo::make_ref<impl::select>();
				WAWO_ALLOC_CHECK(rt, sizeof(impl::select));
			}
			break;
#endif
#ifdef WAWO_ENABLE_WCP
			case T_WPOLL:
			{
				rt = wawo::make_ref<impl::wpoll>();
				WAWO_ALLOC_CHECK(rt, sizeof(impl::wpoll));
			}
			break;
#endif
			default:
			{
				WAWO_THROW("invalid poll type");
			}
			}

			WAWO_ASSERT(rt != NULL);
			return rt;
		}

		io_event_loop_group::io_event_loop_group()
			:m_curr_sys(0), m_curr_wpoll(0)
		{}
		io_event_loop_group::~io_event_loop_group() {}

		void io_event_loop_group::init(int wpoller_count) {
			int sys_i = std::thread::hardware_concurrency();
			//sys_i = 1;

			while (sys_i-- > 0) {
				WWRP<io_event_loop> o = make_poller_by_type(get_poll_type());
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_pollers.push_back(o);
			}

#ifdef WAWO_ENABLE_WCP
			wpoller_count = WAWO_MIN(wpoller_count, 4);
			wpoller_count = WAWO_MAX(wpoller_count, 1);

			if (wpoller_count > 0) {
				wcp::instance()->start();
			}

			while (wpoller_count-- > 0) {
				WWRP<io_event_loop> o = make_poller_by_type(T_WPOLL);
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_wpoll_pollers.push_back(o);
			}
#endif
		}
		WWRP<io_event_loop> io_event_loop_group::next(bool const& return_wpoller) {
#ifdef WAWO_ENABLE_WCP
			if (return_wpoller) {
				int i = m_curr_wpoll.load() % m_wpoll_pollers.size();
				wawo::atomic_increment(&m_curr_wpoll);
				return m_wpoll_pollers[i% m_wpoll_pollers.size()];
			}
			else {
#endif
				int i = m_curr_sys.load() % m_pollers.size();
				wawo::atomic_increment(&m_curr_sys);
				return m_pollers[i% m_pollers.size()];
#ifdef WAWO_ENABLE_WCP
			}
#endif
		}

		void io_event_loop_group::deinit() {
#ifdef WAWO_ENABLE_WCP
			if (m_wpoll_pollers.size()) {
				wcp::instance()->stop();
				std::for_each(m_wpoll_pollers.begin(), m_wpoll_pollers.end(), [](WWRP<io_event_loop> const& o) {
					o->stop();
				});
				m_wpoll_pollers.clear();
			}
#endif
			std::for_each(m_pollers.begin(), m_pollers.end(), [](WWRP<io_event_loop> const& o) {
				o->stop();
			});
			m_pollers.clear();
		}

	}
}