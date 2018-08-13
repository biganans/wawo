#include <wawo/core.hpp>
#ifdef WAWO_ENABLE_EPOLL
#include <wawo/net/poller_impl/epoll.hpp>
#elif defined(WAWO_IO_MODE_IOCP)
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

	inline static WWRP<io_event_loop> make_poller_by_type(io_poller_type t) {
			WWRP<io_event_loop> poller;
			switch (t) {
#ifdef WAWO_ENABLE_EPOLL
			case T_EPOLL:
			{
				poller = wawo::make_ref<impl::epoll>();
				WAWO_ALLOC_CHECK(m_poller, sizeof(impl::epoll));
			}
			break;
#elif defined(WAWO_IO_MODE_IOCP)
			case T_IOCP:
			{
				poller = wawo::make_ref<impl::iocp>();
				WAWO_ALLOC_CHECK(poller, sizeof(impl::iocp));
			}
			break;
#else
			case T_SELECT:
			{
				poller = wawo::make_ref<impl::select>();
				WAWO_ALLOC_CHECK(poller, sizeof(impl::select));
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

			WAWO_ASSERT(poller != NULL);
			return poller;
		}

		io_event_loop_group::io_event_loop_group()
		{}
		io_event_loop_group::~io_event_loop_group() {}

		void io_event_loop_group::init(int wpoller_count) {

			int sys_i = std::thread::hardware_concurrency();
//			sys_i = 1;
			const io_poller_type t = get_poller_type();
			m_curr_idx[t] = 0;
			while (sys_i-- > 0) {
				WWRP<io_event_loop> o = make_poller_by_type(t);
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_pollers[t].push_back(o);
			}

#ifdef WAWO_ENABLE_WCP
			wpoller_count = WAWO_MIN(wpoller_count, 4);
			wpoller_count = WAWO_MAX(wpoller_count, 1);

			if (wpoller_count > 0) {
				wcp::instance()->start();
			}
			m_curr_idx[T_WPOLL] = 0;
			while (wpoller_count-- > 0) {
				WWRP<io_event_loop> o = make_poller_by_type(T_WPOLL);
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_pollers[T_WPOLL].push_back(o);
			}
#else
			(void)wpoller_count;
#endif
		}

		void io_event_loop_group::deinit() {
			for (::size_t i = 0; i < T_POLLER_MAX; ++i) {
				std::for_each(m_pollers[i].begin(), m_pollers[i].end(), [](WWRP<io_event_loop> const& o) {
					o->stop();
				});
				m_pollers[i].clear();
			}
		}

		WWRP<io_event_loop> io_event_loop_group::next(io_poller_type const& t) {
			const io_event_loop_vector& pollers = m_pollers[t];
			return pollers[wawo::atomic_increment(&m_curr_idx[t]) % pollers.size()];
		}

		void io_event_loop_group::execute(fn_io_event_task&& f) {
			next(get_poller_type())->execute(std::forward<fn_io_event_task>(f));
		}
		void io_event_loop_group::schedule(fn_io_event_task&& f) {
			next(get_poller_type())->schedule(std::forward<fn_io_event_task>(f));
		}
	}
}