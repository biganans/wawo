#if WAWO_ISGNU
#include <wawo/net/poller_impl/epoll.hpp>
#endif
#include <wawo/net/poller_impl/select.hpp>
#include <wawo/net/poller_impl/wpoll.hpp>

#include <wawo/net/io_event_loop.hpp>
#include <wawo/net/wcp.hpp>

namespace wawo { namespace net {

	void io_event_loop::init_observer() {
		switch (m_poller_type) {
		case T_SELECT:
		{
			m_observer = wawo::make_ref<impl::select>();
			WAWO_ALLOC_CHECK(m_observer, sizeof(impl::select));
		}
		break;
#if WAWO_ISGNU
		case T_EPOLL:
		{
			m_observer = wawo::make_ref<impl::epoll>();
			WAWO_ALLOC_CHECK(m_observer, sizeof(impl::epoll));
		}
		break;
#endif
		case T_WPOLL:
		{
			m_observer = wawo::make_ref<impl::wpoll>();
			WAWO_ALLOC_CHECK(m_observer, sizeof(impl::wpoll));
		}
		break;
		default:
			{
				WAWO_THROW("invalid poll type");
			}
		}

		WAWO_ASSERT(m_observer != NULL);
		m_observer->init();
	}

	void io_event_loop::deinit_observer() {
		WAWO_ASSERT(m_observer != NULL);
		m_observer->deinit();
		m_observer = NULL;
	}



	io_event_loop_group::io_event_loop_group()
		:m_curr_sys(0), m_curr_wpoll(0)
	{}
	io_event_loop_group::~io_event_loop_group() {}
	
	void io_event_loop_group::init(int wpoller_count ) {
		int i = std::thread::hardware_concurrency();
		int sys_i = i - wpoller_count;
		if (sys_i <= 0) {
			sys_i = 1;
		}

		while (sys_i-- > 0) {
			WWRP<io_event_loop> o = wawo::make_ref<io_event_loop>();
			int rt = o->start();
			WAWO_ASSERT(rt == wawo::OK);
			m_event_loops.push_back(o);
		}

		wpoller_count = WAWO_MIN(wpoller_count, 4);
		wpoller_count = WAWO_MAX(wpoller_count, 1);

		if (wpoller_count > 0) {
			wcp::instance()->start();
		}

		while (wpoller_count-- > 0) {
			WWRP<io_event_loop> o = wawo::make_ref<io_event_loop>(T_WPOLL);
			int rt = o->start();
			WAWO_ASSERT(rt == wawo::OK);
			m_wpoll_event_loops.push_back(o);
		}
	}
	WWRP<io_event_loop> io_event_loop_group::next(bool const& return_wpoller ) {
		if (return_wpoller) {
			int i = m_curr_wpoll.load() % m_wpoll_event_loops.size();
			wawo::atomic_increment(&m_curr_wpoll);
			return m_wpoll_event_loops[i% m_wpoll_event_loops.size()];
		}
		else {
			int i = m_curr_sys.load() % m_event_loops.size();
			wawo::atomic_increment(&m_curr_sys);
			return m_event_loops[i% m_event_loops.size()];
		}
	}

	void io_event_loop_group::deinit() {
		if (m_wpoll_event_loops.size()) {
			wcp::instance()->stop();
			std::for_each(m_wpoll_event_loops.begin(), m_wpoll_event_loops.end(), [](WWRP<io_event_loop> const& o) {
				o->stop();
			});
			m_wpoll_event_loops.clear();
		}

		std::for_each(m_event_loops.begin(), m_event_loops.end(), [](WWRP<io_event_loop> const& o) {
			o->stop();
		});
		m_event_loops.clear();
	}
}}