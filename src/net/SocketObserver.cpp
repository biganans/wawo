#include <wawo/net/SocketObserver.hpp>
#include <wawo/net/NetEvent.hpp>
#include <wawo/net/Socket.hpp>

#ifdef WAWO_IO_MODE_EPOLL
	#include <wawo/net/core/observer_impl/Epoll.hpp>
#elif defined WAWO_IO_MODE_SELECT
	#include <wawo/net/core/observer_impl/Select.hpp>
#else
	#error
#endif

namespace wawo { namespace net {

	void IOEventTask::Run() {

		WAWO_ASSERT(Isset());
		WAWO_ASSERT(m_socket->IsNonBlocking());
		WAWO_ASSERT(m_observer != NULL);

		u32_t const& eid = Event::GetId();
		switch (eid) {
		case IE_CAN_READ:
			{
				m_socket->HandleAsyncPump();
			}
			break;
		case IE_CAN_WRITE:
			{
				//what does ec used for ?
				int flush_ec;
				u32_t left;
				u32_t flushed = m_socket->HandleAsyncFlush(left, flush_ec);

				if (left == 0) {
					m_observer->UnWatch(m_socket, IOE_WR);
				}

				switch (flush_ec) {
				case wawo::E_SOCKET_SEND_IO_BLOCK:
				case wawo::OK:
					{
					}
					break;
				case wawo::E_SOCKET_WR_SHUTDOWN_ALREADY:
					{
						WAWO_WARN("[socket_observer][#%d:%s]async send error: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), flush_ec);
						m_observer->UnWatch(m_socket, IOE_WR);
					}
					break;
				case wawo::E_SOCKET_NOT_CONNECTED:
					{
						WAWO_WARN("[socket_observer][#%d:%s]async send error: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), flush_ec);
						m_observer->UnWatch(m_socket, IOE_ALL);
					}
					break;
				case wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED:
				default:
					{
						WAWO_WARN("[socket_observer][#%d:%s]async send error: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), flush_ec);
						m_observer->UnWatch(m_socket, IOE_ALL);
						int recv_ec;

						do {
							WWSP<wawo::algorithm::Packet> arrives[5];
							u32_t count = m_socket->ReceivePackets(arrives, 5, recv_ec,F_RCV_ALWAYS_PUMP);
							for (u32_t i = 0; i < count; i++) {
								WWRP<SocketEvent> sevt(new SocketEvent(m_socket, SE_PACKET_ARRIVE, arrives[i]));
								m_socket->Trigger(sevt);
							}
						} while (recv_ec == wawo::E_SOCKET_PUMP_TRY_AGAIN);

						WAWO_WARN("[socket_observer][#%d:%s]async send error: %d, shutdown", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), flush_ec);
						m_socket->Shutdown(SHUTDOWN_RDWR, flush_ec);
					}
					break;
				}
				(void)flushed;
			}
			break;
		case IE_CAN_ACCEPT:
			{
				int ec;
				do {
					WWRP<Socket> sockets[WAWO_MAX_ACCEPTS_ONE_TIME];
					u32_t count = m_socket->Accept(sockets, WAWO_MAX_ACCEPTS_ONE_TIME, ec);
					for (u32_t i = 0; i<count; i++) {
						WWRP<SocketEvent> evt(new SocketEvent(m_socket, SE_ACCEPTED, core::Cookie((void*)(sockets[i].Get()))));
						m_socket->Trigger(evt);
					}
				} while (ec == wawo::E_TRY_AGAIN);

				if (ec != wawo::OK) {
					WAWO_WARN("[socket_observer][#%d:%s] shutdown listen socket, ec: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), ec);
					WWRP<SocketEvent> sevt(new SocketEvent(m_socket, SE_ERROR, core::Cookie(ec)));
					m_socket->OSchedule(sevt);
				}
			}
			break;
		case IE_CONNECTED:
			{
				WAWO_ASSERT(m_socket->IsActive());
				int ec;
				m_socket->HandleAsyncConnected(ec);
				m_observer->UnWatch(m_socket, IOE_WR);
				if (ec == wawo::OK) {
					WWRP<SocketEvent> sevt(new SocketEvent(m_socket, SE_CONNECTED));
					m_socket->OSchedule(sevt);
				} else {
					m_socket->Close(ec);
				}
			}
			break;
		case IE_ERROR:
			{
				WAWO_WARN("[socket_observer][#%d:%s] socket error: %d", m_socket->GetFd(), m_socket->GetRemoteAddr().AddressInfo().CStr(), GetCookie().int32_v);
				WAWO_ASSERT(m_socket != NULL);
				m_socket->Close(GetCookie().int32_v);
			}
			break;
		default:
			{
				WAWO_THROW_EXCEPTION("invalid IOEvent id");
			}
			break;
		}
	}

	void IOEventTask::Cancel() {}

	SocketObserver::SocketObserver( bool auto_start ) :
		m_mutex(),
		m_state(S_IDLE),
		m_impl(NULL),
		m_ops_mutex(),
		m_ops()
	{
		if( auto_start ) {
			WAWO_CONDITION_CHECK( Start() == wawo::OK );
		}
	}
	SocketObserver::~SocketObserver() {
		LockGuard<SpinMutex> lg(m_ops_mutex);
		Stop();

		WAWO_ASSERT(m_ops.size() == 0);
		WAWO_ASSERT( m_impl == NULL );
	}

	void SocketObserver::AllocImpl() {
		WAWO_ASSERT( m_impl == NULL );
		m_impl = new core::observer_impl::SocketObserver_Impl(this);
		WAWO_NULL_POINT_CHECK( m_impl );
	}
	void SocketObserver::DeAllocImpl() {
		WAWO_ASSERT( m_impl != NULL );
		WAWO_DELETE( m_impl );
	}
	void SocketObserver::Stop() {
		{
			LockGuard<SharedMutex> _lg( m_mutex );
			m_state = S_EXIT ;
		}
		ThreadRunObject_Abstract::Stop();
	}

	void SocketObserver::OnStart() {
		LockGuard<SharedMutex> _lg( m_mutex );
		WAWO_ASSERT( m_state == S_IDLE );
		m_state = S_RUN ;
		AllocImpl();
		WAWO_ASSERT( m_impl != NULL );
		m_impl->Init();
	}
	void SocketObserver::OnStop() {
		SharedLockGuard<SharedMutex> _lg( m_mutex );
		WAWO_ASSERT( m_state == S_EXIT );
		{
			LockGuard<SpinMutex> oplg( m_ops_mutex );
			while(m_ops.size()) {
				m_ops.pop();
			}
		}

		WAWO_ASSERT( m_impl != NULL );
		m_impl->Deinit();
		DeAllocImpl();
		WAWO_ASSERT( m_state == S_EXIT );
	}

	void SocketObserver::Run() {
		{
			SharedLockGuard<SharedMutex> _lg( m_mutex );
			WAWO_ASSERT( m_impl != NULL );

			switch( m_state ) {
			case S_RUN:
				{
					_ProcessOps();
					m_impl->CheckSystemIO();
					_ProcessOps();
					m_impl->CheckCustomIO();
				}
				break;
			case S_IDLE:
			case S_EXIT:
				{
				}
				break;
			}
		}

//#define WAWO_IO_PERFORMANCE_HIGH
#if defined(WAWO_PLATFORM_WIN) && defined(WAWO_IO_PERFORMANCE_HIGH)
		wawo::yield();
#else
		wawo::usleep(16);
#endif
	}

	void SocketObserver::_ProcessOps() {
		if( m_ops.empty() ) {
			return ;
		}

		WAWO_ASSERT( m_impl != NULL );
		WAWO_ASSERT( m_state == S_RUN );

		LockGuard<SpinMutex> lg(m_ops_mutex);
		while( !m_ops.empty() ) {
			EventOp op = m_ops.front();
			m_ops.pop();

			WAWO_ASSERT( op.socket->IsNonBlocking() );

			switch( op.op ) {
			case OP_WATCH:
				{
					m_impl->Watch( op.socket, op.flag );
				}
				break;
			case OP_UNWATCH:
				{
					m_impl->UnWatch( op.socket, op.flag );
				}
				break;
			}
		}
	}
}}