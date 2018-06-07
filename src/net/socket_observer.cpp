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
					m_impl->watch(op.flag, op.fd, op.fn, op.err);
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

/*
	namespace observer_impl {
		
		void io_select_task::run() {
			WAWO_ASSERT(ctx != NULL);
			if (ctx->poll_type == T_SELECT) {
				switch (id) {
				case IOE_READ:
				{
//					lock_guard<spin_mutex> lg_ctx(ctx->r_mutex);
//					WAWO_ASSERT(ctx->r_state == S_READ_POSTED);
//					ctx->r_state = S_READING;
					task::run();
//					ctx->r_state = S_READ_IDLE;
				}
				break;
				case IOE_WRITE:
				{
//					lock_guard<spin_mutex> lg_ctx(ctx->r_mutex);
//					WAWO_ASSERT(ctx->w_state == S_WRITE_POSTED);
//					ctx->w_state = S_WRITING;
					task::run();
//					ctx->w_state = S_WRITE_IDLE;
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
		
	}*/
}}