#include <wawo/net/observer_impl/select.hpp>

#if WAWO_ISGNU
#include <wawo/net/observer_impl/epoll.hpp>
#endif

#include <wawo/net/observer_impl/wpoll.hpp>
#include <wawo/net/socket_observer.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	void socket_observer::_alloc_impl() {
		WAWO_ASSERT(m_impl == NULL);

		switch (m_polltype) {
		case T_SELECT:
		{
			m_impl = wawo::make_shared<observer_impl::select>();
			WAWO_ALLOC_CHECK(m_impl, sizeof(observer_impl::select));
		}
		break;
#if WAWO_ISGNU
		case T_EPOLL:
		{
			m_impl = wawo::make_shared<observer_impl::epoll>();
			WAWO_ALLOC_CHECK(m_impl, sizeof(observer_impl::epoll));
		}
		break;
#endif
		case T_WPOLL:
		{
			m_impl = wawo::make_shared<observer_impl::wpoll>();
			WAWO_ALLOC_CHECK(m_impl, sizeof(observer_impl::wpoll));
		}
		break;
		default:
			{
				WAWO_THROW("invalid poll type");
			}
		}

	}

	void socket_observer::_dealloc_impl() {
		WAWO_ASSERT(m_impl != NULL);
		m_impl = NULL;
	}

	socket_observer::socket_observer(u8_t const& type ):
		m_impl(NULL),
		m_ops_mutex(),
		m_ops(),
		m_polltype(type)
	{
	}

	socket_observer::~socket_observer() {
		WAWO_ASSERT( m_impl == NULL );
	}

	void socket_observer::init() {
		WAWO_ASSERT(m_impl == NULL);
		_alloc_impl();
		WAWO_ASSERT( m_impl != NULL );
		m_impl->init();
	}
	void socket_observer::deinit() {

		{
			lock_guard<spin_mutex> oplg( m_ops_mutex );
			while(m_ops.size()) {
				m_ops.pop();
			}
		}

		WAWO_ASSERT( m_impl != NULL );
		m_impl->deinit();

		_dealloc_impl();
		WAWO_ASSERT(m_impl == NULL);
	}

	void socket_observer::update() {
		WAWO_ASSERT( m_impl != NULL );
		_process_ops();
		m_impl->check_ioe();
	}

	void socket_observer::_process_ops() {
		if( m_ops.empty() ) {
			return ;
		}

		WAWO_ASSERT( m_impl != NULL );

		lock_guard<spin_mutex> lg(m_ops_mutex);
		while( !m_ops.empty() ) {
			event_op op = m_ops.front();
			m_ops.pop();

			switch( op.op ) {
			case OP_WATCH:
				{
					m_impl->watch(op.flag, op.fd,op.cookie, op.fn, op.err);
				}
				break;
			case OP_UNWATCH:
				{
					m_impl->unwatch(op.flag, op.fd);
				}
				break;
			}
		}
	}

	void observer::watch( u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err )
	{
		m_observer->watch(flag, fd, cookie, fn, err);
	}

	void observer::unwatch(u8_t const& flag, int const& fd)
	{
		m_observer->unwatch(flag, fd);
	}

	namespace observer_impl {
		void io_select_task::run() {
			WAWO_ASSERT(ctx != NULL);
			if (ctx->poll_type == T_SELECT) {
				switch (id) {
				case IOE_READ:
				{
					lock_guard<spin_mutex> lg_ctx(ctx->r_mutex);
					WAWO_ASSERT(ctx->r_state == S_READ_POSTED);
					ctx->r_state = S_READING;
					task::run();
					ctx->r_state = S_READ_IDLE;
				}
				break;
				case IOE_WRITE:
				{
					lock_guard<spin_mutex> lg_ctx(ctx->r_mutex);
					WAWO_ASSERT(ctx->w_state == S_WRITE_POSTED);
					ctx->w_state = S_WRITING;
					task::run();
					ctx->w_state = S_WRITE_IDLE;
				}
				break;
				default:
				{
					WAWO_THROW("unknown ioe id");
				}
				break;
				}
			}
			else {
				task::run();
			}
		}
	}

	void observers::init(int wpoller_count) {
		int i = std::thread::hardware_concurrency();
		int sys_i = i - wpoller_count;
		if (sys_i <= 0) {
			sys_i = 1;
		}

		while (sys_i-- > 0) {
			WWRP<observer> o = wawo::make_ref<observer>();
			int rt = o->start();
			WAWO_ASSERT(rt == wawo::OK);
			m_observers.push_back(o);
		}

		wpoller_count = WAWO_MIN(wpoller_count, 4);
		wpoller_count = WAWO_MAX(wpoller_count, 1);

		if (wpoller_count > 0) {
			wcp::instance()->start();
		}

		while (wpoller_count-- > 0) {
			WWRP<observer> o = wawo::make_ref<observer>(T_WPOLL);
			int rt = o->start();
			WAWO_ASSERT(rt == wawo::OK);
			m_wpolls.push_back(o);
		}
	}

	WWRP<observer> observers::next(bool const& return_wpoller ) {
		if (return_wpoller) {
			int i = m_curr_wpoll.load() % m_wpolls.size();
			wawo::atomic_increment(&m_curr_wpoll);
			return m_wpolls[i% m_wpolls.size()];
		}
		else {
			int i = m_curr_sys.load() % m_observers.size();
			wawo::atomic_increment(&m_curr_sys);
			return m_observers[i% m_observers.size()];
		}
	}

	void observers::deinit() {
		if (m_wpolls.size()) {
			wcp::instance()->stop();
			std::for_each(m_wpolls.begin(), m_wpolls.end(), [](WWRP<observer> const& o) {
				o->stop();
			});
			m_wpolls.clear();
		}

		std::for_each(m_observers.begin(), m_observers.end(), [](WWRP<observer> const& o) {
			o->stop();
		});
		m_observers.clear();
	}
}}