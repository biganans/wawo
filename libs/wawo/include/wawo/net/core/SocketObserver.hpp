#ifndef _WAWO_NET_CORE_SOCKET_OBSERVER_HPP_
#define _WAWO_NET_CORE_SOCKET_OBSERVER_HPP_

#include <wawo/core.h>
#include <wawo/thread/Thread.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/net/core/NetEvent.hpp>
#include <wawo/task/Task.hpp>
#include <wawo/net/core/Socket.h>


namespace wawo { namespace net { namespace core {

	//8 bytes
	template <class _SocketT>
	struct SocketAndEventConfigPair {
		WAWO_REF_PTR<_SocketT> socket ;
		uint16_t flag;
		uint16_t info; //not used right now.
	};

}}}


#ifdef WAWO_IO_MODE_EPOLL
	#include <wawo/net/core/observer_impl/SocketObserver_Epoll.hpp>
#elif defined WAWO_IO_MODE_SELECT
	#include <wawo/net/core/observer_impl/SocketObserver_Select.hpp>
#else
	#error
#endif

namespace wawo { namespace net { namespace core {

	using namespace wawo::thread;

	template <class _SocketT>
	class SocketObserver;

	template <class _SocketT>
	class SocketEventTask:
		public wawo::task::Task_Abstract {

			typedef _SocketT MySocketT;
			typedef typename _SocketT::SocketEventT MySocketEventT;
			typedef IOEvent<MySocketT> MyIOEventT;
			typedef SocketObserver<MySocketT> MySocketObserverT;
			typedef SocketEventTask<_SocketT> MyT;

			WAWO_REF_PTR<MySocketT> m_socket;
			WAWO_REF_PTR<MyIOEventT> m_ioe;
		public:
			explicit SocketEventTask( WAWO_REF_PTR<MySocketT> const& socket, WAWO_REF_PTR<MyIOEventT> const& evt, wawo::task::Priority const& priority = wawo::task::P_NORMAL ):
				Task_Abstract(priority),
				m_socket(socket),
				m_ioe(evt)
			{
			}
			bool Isset() {
				return (m_socket != NULL) && (m_ioe != NULL);
			}
			void Reset() {
				WAWO_ASSERT( Isset() );
				m_socket = NULL;
				m_ioe = NULL;
			}
			void Run() {

				WAWO_ASSERT( Isset() );
				WAWO_ASSERT( m_socket->IsNonBlocking() );

				WAWO_REF_PTR<MySocketObserverT> const& observer = m_ioe->GetObserver() ;
				WAWO_ASSERT( observer != NULL );

				uint32_t const& id = m_ioe->GetId();

				switch( id ) {
				case IE_CAN_READ:
					{
						int ec;
						LockGuard<SpinMutex> lg( m_socket->GetLock(Socket::L_READ) );
						m_socket->HandleAsyncRecv(ec);
						switch( ec ) {
						case wawo::E_SOCKET_RECV_IO_BLOCK:
						case wawo::OK:
							{
							}
							break;
						case wawo::E_SOCKET_NOT_CONNECTED:
							{
								//shutdown already
								observer->UnRegister(SocketObserverEvent::EVT_ALL, m_socket );
							}
							break;
						case wawo::E_SOCKET_REMOTE_GRACE_CLOSE:
						case wawo::E_SOCKET_RECV_BUFFER_FULL:
						default:
							{
								if( wawo::E_SOCKET_REMOTE_GRACE_CLOSE == ec ) {
									WAWO_LOG_DEBUG("socket_observer", "[%d:%s] socket remote gracely close", m_socket->GetFd(), m_socket->GetAddr().AddressInfo() );
								}
								observer->UnRegister(SocketObserverEvent::EVT_ALL, m_socket );
								m_socket->Flush(2000);

								WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_SHUTDOWN, EventData(ec) ) );
								m_socket->Trigger( evt_c );
								m_socket->Shutdown(Socket::SSHUT_RDWR, ec);
							}
							break;
						}
					}
					break;
				case IE_CAN_WRITE:
					{
						//what does ec used for ?
						int ec;
						uint32_t left;
						LockGuard<SpinMutex> lg( m_socket->GetLock(Socket::L_WRITE) );
						m_socket->HandleAsyncSend(left,ec);
						if( left == 0 ) {
							observer->UnRegister( SocketObserverEvent::EVT_WRITE, m_socket );
						}

						if( ec == wawo::E_SOCKET_SEND_IO_BLOCK_EXPIRED ) {
							observer->UnRegister(SocketObserverEvent::EVT_ALL, m_socket );

							m_socket->Pump(ec);

							WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_SHUTDOWN, EventData(ec) ) );
							m_socket->Trigger( evt_c );
							m_socket->Shutdown(Socket::SSHUT_WR, ec );
						}
					}
					break;
				case IE_CAN_ACCEPT:
					{
						int ec;
						LockGuard<SpinMutex> lg( m_socket->GetLock(Socket::L_READ) );
						m_socket->HandleAsyncAccept(ec);

						if(ec != wawo::OK) {
							m_socket->Trigger( WAWO_REF_PTR<MySocketEventT>( new MySocketEventT( m_socket, SE_ERROR, EventData(ec) ) ) );
							WAWO_LOG_WARN("socket_observer", "[%d:%s] shutdown listen socket, ec: %d" , m_socket->GetFd(), m_socket->GetAddr().AddressInfo(), ec );
						}
					}
					break;
				case IE_CONNECTED:
					{
						int ec;
						LockGuard<SpinMutex> lg( m_socket->GetLock( Socket::L_WRITE ) );
						m_socket->HandleAsyncConnected(ec);
						if( m_socket->IsActive() ) {
							observer->UnRegister( SocketObserverEvent::EVT_WRITE, m_socket );
						}

						if( ec == wawo::OK ) {
							WAWO_REF_PTR<MySocketEventT> evt( new MySocketEventT( m_socket, SE_CONNECTED )) ;
							m_socket->Trigger( evt);
						} else {
							WAWO_REF_PTR<MySocketEventT> evt ( new MySocketEventT( m_socket, SE_ERROR, EventData(ec) ) );
							m_socket->Trigger( evt );
							m_socket->Close(ec);
						}
					}
					break;
				case IE_ERROR:
					{
						int ec = m_ioe->GetEventData().int32_v;
						WAWO_ASSERT( ec != wawo::OK );
						WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_ERROR, EventData(ec) ) );
						m_socket->Trigger( evt_c );
					}
					break;
				case IE_CLOSE:
					{
						int ec = m_ioe->GetEventData().int32_v;
						WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_CLOSE, EventData(ec) ) );
						m_socket->Trigger( evt_c );
						m_socket->Close(ec);
					}
					break;
				case IE_SHUTDOWN:
					{
						int ec = m_ioe->GetEventData().int32_v;
						WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_SHUTDOWN, EventData(ec) ) );
						m_socket->Trigger( evt_c );
						m_socket->Shutdown(Socket::SSHUT_RDWR, ec);
					}
					break;
				case IE_SHUTDOWN_RD:
					{
						int ec = m_ioe->GetEventData().int32_v;
						WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_SHUTDOWN_RD, EventData(ec) ) );
						m_socket->Trigger( evt_c );
						m_socket->Shutdown(Socket::SSHUT_RD, ec);
					}
					break;
				case IE_SHUTDOWN_WR:
					{
						int ec = m_ioe->GetEventData().int32_v;
						WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( m_socket, SE_SHUTDOWN_WR, EventData(ec) ) );
						m_socket->Trigger( evt_c );
						m_socket->Shutdown(Socket::SSHUT_WR, ec);
					}
					break;
				default:
					{
						WAWO_THROW_EXCEPTION("invalid IOEvent id");
					}
					break;
				}
			}

			void Cancel() {
				//
			}
		};

	template <class _SocketT>
	class SocketObserver:
		public ThreadRunObject_Abstract,
		public wawo::RefObject_Abstract
	{

		typedef _SocketT MySocketT;
		typedef IOEvent<_SocketT> MyIOEventT;
		typedef SocketEventTask<_SocketT> MySocketEventTask;
		typedef SocketObserver<_SocketT> MyT;

		typedef observer_impl::SocketObserver_Impl<_SocketT> MySocketObserverT;
		enum EventOpCode {
			OP_REGISTER,
			OP_UNREGISTER
		};

		struct EventOp {
			WAWO_REF_PTR<MySocketT> socket;
			uint16_t flag;
			EventOpCode op:16;
		};
		typedef std::queue<EventOp> EventOpQueue ;

		SpinMutex m_ops_mutex;
		EventOpQueue m_ops;

		enum SocketObserverState {
			S_IDLE = 0,
			S_RUN,
			S_EXIT
		};

	public:
		SocketObserver( bool auto_start=false ) :
			m_state(S_IDLE),
			m_impl(NULL)
		{
			AllocImpl();

			if( auto_start ) {
				WAWO_CONDITION_CHECK( Start() == wawo::OK );
			}
		}
		~SocketObserver() {
			ProcessOps();
			LockGuard<SpinMutex> lg(m_ops_mutex);
			WAWO_ASSERT(m_ops.size() == 0);
			Stop();
			DeAllocImpl();
		}

		void AllocImpl() {
			WAWO_ASSERT( m_impl == NULL );
			m_impl = new MySocketObserverT(this);
			WAWO_NULL_POINT_CHECK( m_impl );
		}
		void DeAllocImpl() {
			WAWO_ASSERT( m_impl != NULL );
			WAWO_DELETE( m_impl );
		}

		void Stop() {
			{
				LockGuard<SharedMutex> _lg( m_mutex );
				m_state = S_EXIT ;
			}

			ThreadRunObject_Abstract::Stop();
		}
		void OnStart() {
			LockGuard<SharedMutex> _lg( m_mutex );
			WAWO_ASSERT( m_state == S_IDLE );
			m_state = S_RUN ;
			WAWO_ASSERT( m_impl != NULL );
			m_impl->Init();
		}
		void OnStop() {
			LockGuard<SharedMutex> _lg( m_mutex );
			WAWO_ASSERT( m_state == S_EXIT );
			ProcessOps();
			WAWO_ASSERT( m_impl != NULL );
			m_impl->Deinit();
		}
		void Run() {
			{
				SharedLockGuard<SharedMutex> _lg( m_mutex );
				switch( m_state ) {
				case S_RUN:
					{
						WAWO_ASSERT( m_impl != NULL );
						ProcessOps();
						m_impl->CheckSystemIO();
						ProcessOps();
						m_impl->CheckCustomIO();
					}
					break;
				case S_IDLE:
					{
					}
					break;
				case S_EXIT:
					{
					}
					break;
				}
			}

#ifdef WAWO_IO_PERFORMACE_SUPER_HIGH
#elif defined(WAWO_IO_PERFORMANCE_HIGH)
			wawo::yield();
#else
			wawo::usleep(1);
#endif
		}

		//add socket to async io check list
		void Register( uint16_t const& flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if( m_state != S_RUN ) {
				return ;
			}

			WAWO_ASSERT( m_impl != NULL );
			AddRegisterOp( flag, socket );
		}
		//remote socket from async io check list for the specified evt_flag
		void UnRegister( uint16_t const& flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if( m_state != S_RUN ) {
				return ;
			}

			WAWO_ASSERT( m_impl != NULL );
			AddUnRegisterOp( flag, socket );
		}

		void AddRegisterOp( uint16_t const& flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( socket->IsNonBlocking() );
			EventOp op = {
				socket,
				flag,
				OP_REGISTER
			};

			LockGuard<SpinMutex> _lg( m_ops_mutex ) ;
			m_ops.push( op );
		}
		void AddUnRegisterOp( uint16_t const& flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( socket->IsNonBlocking() );
			EventOp op = {
				socket,
				flag,
				OP_UNREGISTER
			};
			LockGuard<SpinMutex> _lg( m_ops_mutex ) ;
			m_ops.push( op );
		}

		inline void ProcessOps() {
			if( m_ops.empty() ) {
				return ;
			}

			LockGuard<SpinMutex> lg(m_ops_mutex);
			while( !m_ops.empty() ) {
				EventOp op = m_ops.front();
				m_ops.pop();

				WAWO_ASSERT( op.socket->IsNonBlocking() );

				switch( op.op ) {
				case OP_REGISTER:
					{
						switch( m_state ) {
						case S_RUN:
							{
								m_impl->Register( op.flag,op.socket );
							}
							break;
						case S_EXIT:
							{
							}
							break;
						case S_IDLE:
							{
								WAWO_ASSERT( !"pls check");
							}
							break;
						}
					}
					break;
				case OP_UNREGISTER:
					{
						switch( m_state ) {
						case S_RUN:
							{
								m_impl->UnRegister( op.flag,op.socket );
							}
							break;
						case S_EXIT:
							{
							}
							break;
						case S_IDLE:
							{
								WAWO_ASSERT( !"pls check");
							}
							break;
						}
					}
					break;
				}
			}
		}

		inline void PostIOEvent( WAWO_REF_PTR<MySocketT> const& socket, uint32_t const& evt_id, int const& ec = 0 ) {
			WAWO_REF_PTR<MyIOEventT> io_event( new MyIOEventT( WAWO_REF_PTR<MyT>(this), evt_id,  EventData(ec) ) );
			WAWO_REF_PTR<wawo::task::Task_Abstract> task_evt( new MySocketEventTask(socket,io_event) );
			WAWO_IO_TASK_MANAGER->PlanTask( task_evt );
		}

	private:
		SharedMutex m_mutex;
		SocketObserverState m_state;
		MySocketObserverT* m_impl;
	};

}}} //observer net wawo

//#define WAWO_SOCKET_OBSERVER (wawo::net::core::observer::SocketObserver::GetInstance())
#endif //
