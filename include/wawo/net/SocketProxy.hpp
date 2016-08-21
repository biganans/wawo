#ifndef WAWO_NET_SOCKET_PROXY_HPP_
#define WAWO_NET_SOCKET_PROXY_HPP_

#include <queue>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/thread/Thread.hpp>
#include <wawo/net/core/Listener_Abstract.hpp>

#include <wawo/net/Socket.hpp>
#include <wawo/net/NetEvent.hpp>
#include <wawo/net/SocketObserver.hpp>


#define WAWO_DISABLE_SOCKET_PROXY_THREAD

namespace wawo { namespace net {

	using namespace wawo::thread;

	class SocketProxy :
		public ThreadRunObject_Abstract,
		public core::Listener_Abstract<SocketEvent> //for socket server,and active connect socket
	{
		typedef core::Listener_Abstract<SocketEvent> _ListenerT;


		typedef std::vector< WWRP<Socket> > SocketPool;
		enum OpCode {
			OP_NONE,
			OP_ADD,
			OP_REMOVE,
			OP_WATCH,
			OP_UNWATCH,
			OP_CONNECT,
			OP_ERROR,
			OP_SHUTDOWN_RD,
			OP_SHUTDOWN_WR,
			OP_CLOSE
		};

		struct Op {
			WWRP<Socket> socket;
			OpCode op:8;
			int int_v:24;
		};

		typedef std::queue<Op> OpQueue;

		enum _State {
			S_IDLE,
			S_RUN,
			S_EXIT
		};

	private:
		SharedMutex m_mutex;
		int m_state;

		SpinMutex m_sockets_mutex;
		SocketPool m_sockets;

		SpinMutex m_ops_mutex;
		OpQueue m_ops;
		WWRP<SocketObserver> m_observer;
	public:
		SocketProxy():
			m_state(S_IDLE),
			m_observer(NULL)
		{
		}
		~SocketProxy() {
			WAWO_ASSERT(m_state == S_IDLE || m_state == S_EXIT );
			WAWO_ASSERT(m_observer == NULL);
		}

		inline bool IsRunning() { return m_state == S_RUN ;}
		void Stop() {
			{
				LockGuard<SharedMutex> _lg( m_mutex ) ;
				if( m_state == S_EXIT ) {return ;}
				m_state = S_EXIT ;
			}
			ThreadRunObject_Abstract::Stop();
		}
		void OnStart(){
			LockGuard<SharedMutex> _lg( m_mutex ) ;
			WAWO_ASSERT( m_state == S_IDLE );
			m_state = S_RUN ;

			m_observer = WWRP<SocketObserver>( new SocketObserver() );
			WAWO_NULL_POINT_CHECK(m_observer);

			int rt = m_observer->Start();
			WAWO_CONDITION_CHECK( rt == wawo::OK );
		}

		void OnStop() {
			SharedLockGuard<SharedMutex> _lg( m_mutex ) ;
			WAWO_ASSERT( m_state == S_EXIT );

			_ExecuteOps();
			{
				LockGuard<SpinMutex> lg_sockets(m_sockets_mutex);
				std::for_each( m_sockets.begin(), m_sockets.end(), [this]( WWRP<Socket> const& socket ) {
					socket->Close(wawo::E_SOCKET_PROXY_EXIT);
					Op op = {socket, OP_CLOSE,wawo::E_SOCKET_PROXY_EXIT };
					_PlanOp(op);
				});
			}
			_ExecuteOps();

			WAWO_ASSERT(m_observer != NULL );
			m_observer->Stop();
			m_observer = NULL;

			WAWO_ASSERT( m_sockets.size() == 0 );
			WAWO_ASSERT( m_state == S_EXIT );
		}

		void Run() {
			{
				SharedLockGuard<SharedMutex> lg(m_mutex);
				switch( m_state ) {
				case S_RUN:
					{
						_ExecuteOps();
						LockGuard<SpinMutex> lg_sockets(m_sockets_mutex);
						u32_t c = m_sockets.size();
						for (u32_t i = 0; i < c; i++) {
							if (m_sockets[i]->IsFlushTimerExpired()) {
								m_sockets[i]->Shutdown(wawo::net::SHUTDOWN_RDWR,wawo::E_SOCKET_ASYNC_FLUSH_TIMER_EXPIRED);
							}
						}
					}
					break;
				case S_EXIT:
				case S_IDLE:
					{
					}
					break;
				default:
					{
						WAWO_THROW_EXCEPTION("SocketProxy core PROBLEM !!!");
					}
					break;
				}
			}

			if( m_ops.size() == 0 ) {
				wawo::sleep(2);
			}
		}

		inline void _PlanOp( Op const& op ) {
			LockGuard<SpinMutex> lg(m_ops_mutex);
			m_ops.push(op);
		}

		inline void _PopOp(Op& op) {
			LockGuard<SpinMutex> lg_ops( m_ops_mutex );
			if( m_ops.empty() ) return ;
			op = m_ops.front();
			m_ops.pop();
		}

		inline void _ExecuteOps() {

			WAWO_ASSERT(m_observer != NULL );
			while( !m_ops.empty() ) {

				Op op;
				op.op = OP_NONE;
				_PopOp(op);
				if( op.op == OP_NONE) {break;}

				WWRP<Socket> const& socket	= op.socket ;
				OpCode const& code			= op.op;
				int const& int_v			= op.int_v;

				switch (code) {
				case OP_ADD:
					{
						LockGuard<SpinMutex> lg_sockets(m_sockets_mutex);
						WAWO_ASSERT(socket->IsNonBlocking());
	#ifdef _DEBUG
						SocketPool::iterator it = std::find(m_sockets.begin(), m_sockets.end(), socket);
						WAWO_ASSERT(it == m_sockets.end());
	#endif

						WWRP<_ListenerT> socket_l(this);

						if (socket->IsListener()) {
							socket->Register(SE_ERROR, socket_l);
							socket->Register(SE_CLOSE, socket_l);
						}
						else {
							socket->Register(SE_ERROR, socket_l);
							socket->Register(SE_SEND_BLOCKED, socket_l);
							socket->Register(SE_RD_SHUTDOWN, socket_l);
							socket->Register(SE_WR_SHUTDOWN, socket_l);
							socket->Register(SE_CLOSE, socket_l);
						}
						m_sockets.push_back(socket);
					}
					break;
				case OP_REMOVE:
					{
						LockGuard<SpinMutex> lg_sockets(m_sockets_mutex);

						//sometimes , we may hit several times here..
						//epoll error, checking concurrent with async read error,
						//stop all sockets,and concurrent with error..
						SocketPool::iterator it = std::find(m_sockets.begin(), m_sockets.end(), socket);
						if (it != m_sockets.end()) {
							WWRP<_ListenerT> socket_l(this);
							socket->UnRegister(socket_l);
							m_sockets.erase(it);
							WAWO_ASSERT(m_observer != NULL);
							m_observer->UnWatch(socket, IOE_ALL);
						}
					}
					break;
				case OP_WATCH:
					{
#ifdef _DEBUG
						SocketPool::iterator it = std::find(m_sockets.begin(), m_sockets.end(), socket);
						WAWO_ASSERT(it != m_sockets.end());
#endif
						WAWO_ASSERT(socket->IsNonBlocking());
						WAWO_ASSERT((int_v>0) && (int_v <= IOE_ALL));
						m_observer->Watch(socket, int_v);
					}
					break;
				case OP_UNWATCH:
					{
#ifdef _DEBUG
						SocketPool::iterator it = std::find(m_sockets.begin(), m_sockets.end(), socket);
						WAWO_ASSERT(it != m_sockets.end());
#endif
						WAWO_ASSERT(socket->IsNonBlocking());
						WAWO_ASSERT((int_v>0) && (int_v <= IOE_ALL));
						m_observer->UnWatch(socket, int_v);
					}
					break;
				case OP_CONNECT:
					{
						switch(m_state) {
						case S_RUN:
							{
								int rt = socket->Open();
								if( rt != wawo::OK ) {
									socket->Close(rt);
									break;
								}

								rt = socket->AsyncConnect() ;
								if( rt == wawo::E_SOCKET_CONNECTING ) {
									LockGuard<SpinMutex> lg( socket->GetLock( L_WRITE ) );
									WAWO_ASSERT( socket->GetWState() == S_WRITE_IDLE );
									WAWO_ASSERT( socket->IsConnecting() );

									//socket->AssignObserver( m_socket_observer );
									m_observer->Watch( socket, IOE_WRITE );
									break;
								} else if(rt == wawo::OK ) {
									//connected directly
									WWRP<SocketEvent> evt( new SocketEvent( socket, SE_CONNECTED )) ;
									socket->OSchedule( evt );
									break;
								} else {
									socket->Close(rt);
									break;
								}
							}
							break;
						case S_EXIT:
							{
								socket->Close(wawo::E_SOCKET_PROXY_INVALID_STATE);
							}
							break;
						case S_IDLE:
							{
								WAWO_FATAL("[socket_proxy][#%d:%s] SocketProxy::_ExecuteOps() invalid stat(%d) for OP_ASYNC_SEND", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), m_state );
							}
							break;
						}
					}
					break;
				case OP_CLOSE:
					{
						Op op2 = {socket, OP_REMOVE,int_v};
						_PlanOp(op2);
					}
					break;
				case OP_SHUTDOWN_RD:
					{
						WAWO_ASSERT( socket->IsDataSocket() );
						m_observer->UnWatch( socket, IOE_RD );
						if( socket->IsReadWriteShutdowned() ) {
							socket->Close( int_v );
						}
					}
					break;
				case OP_SHUTDOWN_WR:
					{
						WAWO_ASSERT( socket->IsDataSocket() );

						m_observer->UnWatch( socket, IOE_WR );
						if( socket->IsReadWriteShutdowned() ) {
							socket->Close( int_v );
						}
					}
					break;
				case OP_ERROR:
					{
					//only for async connect fail code
					switch( int_v ) {
						case wawo::E_ECONNRESET:
						case wawo::E_EPIPE:
						case wawo::E_ETIMEOUT:
							{
								socket->Close(int_v);
							}
							break;
						case wawo::E_SOCKET_NOT_CONNECTED:
							{
								//closed already
								WAWO_ASSERT( !"WHAT" );
							}
							break;
						case wawo::E_SOCKET_UNKNOW_ERROR:
							{//do nothing
								WAWO_ASSERT( !"WHAT" );
							}
							break;
						case wawo::E_SOCKET_RD_SHUTDOWN_ALREADY:
							{
								WAWO_ASSERT(m_observer != NULL );
								m_observer->UnWatch( socket, IOE_RD );
								WAWO_DEBUG("[socket_proxy][#%d:%s] socket error, wawo::E_SOCKET_RD_SHUTDOWN_ALREADY", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
							}
							break;
							{
								WAWO_THROW_EXCEPTION( "wawo logic error" );
							}
							break;
						}
					}
					break;
					case OP_NONE:
					{//just for warning ...
					}
					break;
				}
			}
		}

		void OnEvent( WWRP<SocketEvent> const& evt ) {

			SharedLockGuard<SharedMutex> lg(m_mutex);
			int evt_id = evt->GetId();
			if( m_state != S_RUN ) {
				WAWO_ASSERT( m_state == S_EXIT );
				WAWO_ASSERT( evt->GetSocket() != NULL );

				WWRP<_ListenerT> socket_l(this);
				evt->GetSocket()->UnRegister(evt_id, socket_l);

				evt->GetSocket()->Close( wawo::E_SOCKET_PROXY_INVALID_STATE );
				WAWO_WARN("[socket_proxy][#%d:%s] socket exist already, close socket", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr() );
				return ;
			}

			int const& ec = evt->GetCookie().int32_v;
			(void) &ec;

			switch( evt_id ) {
			case SE_SEND_BLOCKED:
				{
					WAWO_ASSERT( evt->GetSocket()->IsDataSocket() );
					WAWO_ASSERT( evt->GetSocket()->IsNonBlocking() );
					WAWO_ASSERT(m_observer != NULL );

					m_observer->Watch( evt->GetSocket(), IOE_WRITE);
				}
				break;
			case SE_CLOSE:
				{
					m_observer->UnWatch(evt->GetSocket(), IOE_ALL);
					Op op = { evt->GetSocket(), OP_CLOSE, ec };
					_PlanOp(op);
				}
				break;
			case SE_RD_SHUTDOWN:
				{
					WAWO_ASSERT(m_observer != NULL );
					m_observer->UnWatch(evt->GetSocket(), IOE_RD);

					WAWO_DEBUG("[socket_proxy][#%d:%s]socket SHUT_RD: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), ec );
					Op op = { evt->GetSocket(), OP_SHUTDOWN_RD, ec };
					_PlanOp(op);
				}
				break;
			case SE_WR_SHUTDOWN:
				{
					m_observer->UnWatch(evt->GetSocket(), IOE_WR);

					WAWO_DEBUG("[socket_proxy][#%d:%s]socket SHUT_WR: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), ec );
					Op op = { evt->GetSocket(), OP_SHUTDOWN_WR, ec };
					_PlanOp(op);
				}
				break;
			case SE_ERROR:
				{
					WAWO_WARN("[socket_proxy][#%d:%s]socket error: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), ec );
					WAWO_ASSERT( ec != wawo::OK );

					Op op = { evt->GetSocket(), OP_ERROR, ec };
					_PlanOp(op);
				}
				break;
			default:
				{
					WAWO_THROW_EXCEPTION("SocketProxy::OnEvent(), unknow socket evt id, wawo socket_proxy logic error") ;
				}
				break;
			}
		}

		int Connect( WWRP<Socket> const& socket, bool const& nonblocking = false ) {
			WAWO_ASSERT( IsRunning() );
			WAWO_ASSERT( socket != NULL);
			WAWO_ASSERT( !socket->IsConnected() );
			if (m_state != S_RUN) return wawo::E_SOCKET_PROXY_INVALID_STATE;

			if (nonblocking == true) {
				Op op = { socket,OP_CONNECT,0 };
				_PlanOp(op);
				return wawo::OK;
			}

			int rt = socket->Open();
			WAWO_RETURN_V_IF_NOT_MATCH( rt, rt == wawo::OK );
			rt = socket->Connect();
			WAWO_RETURN_V_IF_NOT_MATCH(rt, rt == wawo::OK);
			return socket->TLP_Handshake();
		}
		
		inline int AddSocket(WWRP<Socket> const& socket, int const& io_evt_to_watch = 0) {
			WAWO_ASSERT(m_state == S_RUN);
			WAWO_ASSERT(m_observer != NULL);
			WAWO_ASSERT(socket->IsNonBlocking() && !socket->IsIdle());
			SharedLockGuard<SharedMutex> lg(m_mutex);

			if (m_state != S_RUN) return wawo::E_SOCKET_PROXY_INVALID_STATE;

			Op op1 = { socket, OP_ADD, 0 };
			_PlanOp(op1);

			if (io_evt_to_watch != 0) {
				Op op2 = { socket, OP_WATCH, io_evt_to_watch };
				_PlanOp(op2);
			}
			return wawo::OK;
		}

		inline int RemoveSocket(WWRP<Socket> const& socket) {
			WAWO_ASSERT(m_state == S_RUN);
			WAWO_ASSERT(m_observer != NULL);
			WAWO_ASSERT(socket->IsNonBlocking() && !socket->IsIdle());
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if (m_state != S_RUN) return wawo::E_SOCKET_PROXY_INVALID_STATE;

			Op op1 = { socket, OP_UNWATCH, IOE_ALL };
			Op op2 = { socket, OP_REMOVE, 0 };

			_PlanOp(op1);
			_PlanOp(op2);
			return wawo::OK;
		}

		inline int WatchSocket(WWRP<Socket> const& socket, int const& io_evt ) {
			WAWO_ASSERT( m_state == S_RUN );
			WAWO_ASSERT( m_observer != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() && !socket->IsIdle() );
			WAWO_ASSERT(io_evt != 0 );
			if (m_state != S_RUN) return wawo::E_SOCKET_PROXY_INVALID_STATE;

			WAWO_ASSERT((io_evt>0) && (io_evt <= IOE_ALL));
			m_observer->Watch(socket, io_evt);

			return wawo::OK ;
		}

		int UnWatchSocket(WWRP<Socket> const& socket, int const& io_evt ) {
			WAWO_ASSERT( m_state == S_RUN );
			WAWO_ASSERT( socket->IsNonBlocking() && !socket->IsIdle() );
			WAWO_ASSERT(m_observer != NULL );
			WAWO_ASSERT(io_evt != 0);

			if (m_state != S_RUN) return wawo::E_SOCKET_PROXY_INVALID_STATE;

			//Op op = { socket, OP_UNWATCH, IO_EVT };
			//_PlanOp(op);

			WAWO_ASSERT((io_evt>0) && (io_evt <= IOE_ALL));
			m_observer->UnWatch(socket, io_evt);

			return wawo::OK ;
		}
	};

}}
#endif