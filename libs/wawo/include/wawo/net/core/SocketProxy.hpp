#ifndef WAWO_NET_CORE_SOCKET_SERVER_ABSTRACT_HPP_
#define WAWO_NET_CORE_SOCKET_SERVER_ABSTRACT_HPP_

#include <vector>
#include <queue>
#include <map>

#include <wawo/core.h>
#include <wawo/SmartPtr.hpp>
#include <wawo/thread/Thread.h>

#include <wawo/net/core/Socket.h>
#include <wawo/net/core/Listener_Abstract.hpp>

#include <wawo/net/core/NetEvent.hpp>
#include <wawo/net/core/SocketObserver.hpp>

#define WAWO_LISTEN_BACK_LOG 1024

namespace wawo { namespace net { namespace core {

	using namespace wawo::net::core;
	using namespace wawo::thread;

	template <class _SocketT>
	class SocketProxy :
		public ThreadRunObject_Abstract,
		public Listener_Abstract< typename _SocketT::SocketEventT > //for socket server,and active connect socket
	{
		typedef SocketProxy<_SocketT> MyT;
		typedef _SocketT MySocketT;
		typedef typename MySocketT::SocketEventT MySocketEventT;
		typedef Listener_Abstract<MySocketEventT> _MyListenerT;

		typedef std::vector< WAWO_REF_PTR<MySocketT> > SocketPool;
		typedef SocketObserver<MySocketT> MySocketObserverT;

		enum OpCode {
			OP_ADD,
			OP_REMOVE,
			OP_CONNECT,
			OP_START_LISTEN,
			OP_STOP_LISTEN,
			OP_ERROR,
			OP_ASYNC_SEND,
			OP_SHUTDOWN_RD,
			OP_SHUTDOWN_WR,
			OP_SHUTDOWN,
			OP_SHUTDOWN_AND_REMOVE,
			OP_CLOSE,
			OP_CLOSE_AND_REMOVE
		};

		struct Op {
			WAWO_REF_PTR<MySocketT> socket;
			OpCode op:8;
			int ec:24;
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

		SocketPool m_sockets;
		SocketPool m_connecting_sockets;

		SpinMutex m_ops_mutex;
		OpQueue m_ops;
		WAWO_REF_PTR<MySocketObserverT> m_socket_observer;

	public:
		SocketProxy():
			m_state(S_IDLE),
			m_socket_observer(NULL)
		{
			m_socket_observer = WAWO_REF_PTR<MySocketObserverT>( new MySocketObserverT() );
			WAWO_NULL_POINT_CHECK( m_socket_observer );
		}

		~SocketProxy() {
			//in case we forget stop
			Stop();
			m_socket_observer = NULL;
		}

		bool IsRunning() {
			return m_state == S_RUN ;
		}
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

			WAWO_ASSERT( m_socket_observer != NULL );
			int rt = m_socket_observer->Start();
			WAWO_CONDITION_CHECK( rt == wawo::OK );
		}

		void OnStop() {
			LockGuard<SharedMutex> _lg( m_mutex ) ;
			WAWO_ASSERT( m_state == S_EXIT );

			_ExecuteOps();
			std::for_each( m_sockets.begin(), m_sockets.end(), [&]( WAWO_REF_PTR<MySocketT> const& socket ) {
				socket->Flush(1000);
				Op op = {socket, OP_SHUTDOWN_AND_REMOVE, wawo::E_SOCKET_PROXY_EXIT};
				_PlanOp(op);
			});
			_ExecuteOps();

			WAWO_ASSERT( m_socket_observer != NULL );
			m_socket_observer->Stop();
			//it should be safe here, cuz we checked the m_state in _CheckOps
			WAWO_ASSERT( m_state == S_EXIT );
		}

		void Run() {
			{
				LockGuard<SharedMutex> lg(m_mutex);
				switch( m_state ) {
				case S_RUN:
					{
						_ExecuteOps();
					}
					break;
				case S_EXIT:
					{
					}
					break;
				case S_IDLE:
					{
					}
					break;
				default:
					{
						WAWO_THROW_EXCEPTION("SocketProxy m_state check,,, What!!!");
					}
					break;
				}
			}

			if( m_ops.size() == 0 ) {
				wawo::usleep(64);
			}
		}

		inline void _PlanOp( Op const& op ) {
			LockGuard<SpinMutex> lg(m_ops_mutex);
			m_ops.push(op);
		}

		inline void _ExecuteOps() {

			LockGuard<SpinMutex> lg(m_ops_mutex);
			WAWO_ASSERT( m_socket_observer != NULL );

			while( !m_ops.empty() ) {

				Op op = m_ops.front();
				m_ops.pop();

				WAWO_REF_PTR<MySocketT>& socket	= op.socket ;
				OpCode const& code						= op.op;
				int const& ec							= op.ec;

				switch( code ) {
				case OP_ADD:
					{
						switch( m_state ) {
						case S_RUN:
							{
#ifdef _DEBUG
								typename SocketPool::iterator it = std::find(m_sockets.begin(), m_sockets.end(), socket );
								WAWO_ASSERT( it == m_sockets.end() );
#endif
								WAWO_ASSERT( socket->IsNonBlocking() );

								WAWO_REF_PTR<_MyListenerT> socket_l(this);

								if( socket->IsListener() ) {
									socket->Register( SE_ERROR, socket_l );
									socket->Register( SE_CLOSE, socket_l );
								} else {
									socket->Register( SE_ERROR, socket_l );
									socket->Register( SE_SEND_BLOCK, socket_l );
									socket->Register( SE_SHUTDOWN_RD,socket_l );
									socket->Register( SE_SHUTDOWN_WR,socket_l );
									socket->Register( SE_SHUTDOWN, socket_l );
									socket->Register( SE_CLOSE, socket_l );
								}
								m_sockets.push_back(socket);
								m_socket_observer->Register( SocketObserverEvent::EVT_READ, socket );
							}
							break;
						case S_EXIT:
							{
								Op op = {socket,OP_SHUTDOWN, wawo::E_SOCKET_PROXY_INVALID_STATE };
								m_ops.push(op);
							}
							break;
						case S_IDLE:
							{
								WAWO_ASSERT("what!!!");
							}
							break;
						}
					}
					break;
				case OP_REMOVE:
					{
						//sometimes , we may hit several times here..
						//epoll error, checking concurrent with async read error,
						//stop all currentingly sockets,,and concurrent with error..
						typename SocketPool::iterator it = std::find( m_sockets.begin(),m_sockets.end(), socket );
						if( it != m_sockets.end() ) {
							m_sockets.erase(it);
						}
					}
					break;
				case OP_CONNECT:
					{
						switch(m_state) {
						case S_RUN:
							{
								int rt = socket->Open();
								if( rt != wawo::OK ) {
									m_socket_observer->PostIOEvent( socket, IE_ERROR, rt );
									socket->Close(rt);
									break;
								}

								rt = socket->TurnOnNonBlocking();
								if( rt != wawo::OK ) {
									m_socket_observer->PostIOEvent( socket, IE_ERROR, rt );
									socket->Close(rt);
									break;
								}

								rt = socket->Connect() ;
								if( rt == wawo::E_SOCKET_CONNECTING ) {
									LockGuard<SpinMutex> lg( socket->GetLock( Socket::L_WRITE ) );
									WAWO_ASSERT( socket->GetWState() == S_WRITE_IDLE );
									WAWO_ASSERT( socket->IsConnecting() );
									m_socket_observer->Register( SocketObserverEvent::EVT_WRITE, socket );
									break;
								} else if(rt == wawo::OK ) {
									//connected directly
									WAWO_REF_PTR<MySocketEventT> evt( new MySocketEventT( socket, SE_CONNECTED )) ;
									socket->Trigger( evt);
								} else {
									WAWO_REF_PTR<MySocketEventT> evt_c ( new MySocketEventT( socket, SE_ERROR, EventData(rt) ) );
									socket->Trigger( evt_c );
									socket->Close(rt);
									break;
								}
							}
							break;
						case S_EXIT:
							{
								m_socket_observer->PostIOEvent( socket, IE_ERROR, wawo::E_SOCKET_PROXY_EXIT );
							}
							break;
						case S_IDLE:
							{
								WAWO_ASSERT(!"what !!!");
							}
							break;
						}
					}
					break;
				case OP_START_LISTEN:
					{
						int listen = socket->Listen( WAWO_LISTEN_BACK_LOG );

						if( listen == wawo::OK ) {
							Op op = {socket, OP_ADD, wawo::OK };
							m_ops.push(op);
						} else {
							WAWO_ASSERT( m_socket_observer != NULL );
							m_socket_observer->PostIOEvent( socket, IE_ERROR, listen );
							socket->Close(listen);
						}
					}
					break;
				case OP_STOP_LISTEN:
					{
						WAWO_ASSERT( socket->IsListener() );

						typename SocketPool::iterator it = std::find( m_sockets.begin(),m_sockets.end(), socket );
						WAWO_ASSERT( it != m_sockets.end() );

						Op op = {socket, OP_CLOSE_AND_REMOVE, wawo::OK };
						m_ops.push(op);
					}
					break;
				case OP_ASYNC_SEND:
					{
						switch(m_state) {
						case S_RUN:
							{
								m_socket_observer->Register(SocketObserverEvent::EVT_WRITE, socket);
							}
							break;
						}
					}
					break;
				case OP_CLOSE_AND_REMOVE:
					{
						Op op1 = {socket,OP_CLOSE,ec};
						m_ops.push(op1);

						Op op2 = {socket,OP_REMOVE,ec};
						m_ops.push(op2);
					}
					break;
				case OP_SHUTDOWN_AND_REMOVE:
					{
						Op op1 = {socket,OP_SHUTDOWN,ec};
						m_ops.push(op1);

						Op op2 = {socket,OP_REMOVE,ec};
						m_ops.push(op2);
					}
					break;
				case OP_CLOSE:
					{
						WAWO_ASSERT( m_socket_observer != 0 );

						WAWO_REF_PTR<_MyListenerT> socket_l(this);
						socket->UnRegister(socket_l);

						m_socket_observer->PostIOEvent( socket, IE_CLOSE, ec );
					}
					break;
				case OP_SHUTDOWN:
					{
						WAWO_ASSERT( m_socket_observer != 0 );

						WAWO_REF_PTR<_MyListenerT> socket_l(this);
						socket->UnRegister(socket_l);

						m_socket_observer->PostIOEvent( socket, IE_SHUTDOWN, ec );
					}
					break;
				case OP_SHUTDOWN_RD:
				case OP_SHUTDOWN_WR:
					{
						//USELESS FOR PROXY NOW...
					}
					break;
				case OP_ERROR:
					{
						//only for async connect fail code
						switch( ec ) {
							case wawo::E_ECONNRESET:
							case wawo::E_ECONNABORTED:
							case wawo::E_WSAECONNABORTED:
							case wawo::E_WSAECONNRESET:
								{

									Op op = { socket, OP_SHUTDOWN_AND_REMOVE, ec };
									m_ops.push(op);
								}
								break;
							case wawo::E_EPIPE:
							case wawo::E_SOCKET_NOT_CONNECTED:
								{
									//closed already
								}
								break;
							case wawo::E_SOCKET_UNKNOW_ERROR:
								{//do nothing
								}
								break;
							default:
								{
									WAWO_THROW_EXCEPTION( "wawo logic error" );
								}
								break;
						}
					}
					break;
				}
			}
		}

		void OnEvent( WAWO_REF_PTR<MySocketEventT> const& evt ) {
			LockGuard<SharedMutex> lg(m_mutex);
			int evt_id = evt->GetId();
			if( m_state != S_RUN ) {
				WAWO_ASSERT( m_state == S_EXIT );
				WAWO_ASSERT( evt->GetSocket() != NULL );

				WAWO_REF_PTR<_MyListenerT> socket_l(this);
				evt->GetSocket()->UnRegister(evt_id, socket_l);

				m_socket_observer->PostIOEvent( evt->GetSocket(), IE_ERROR, wawo::E_SOCKET_PROXY_INVALID_STATE );
				return ;
			}

			switch( evt_id ) {
			case SE_SEND_BLOCK:
				{
					WAWO_ASSERT( evt->GetSocket()->IsDataSocket() );
					WAWO_ASSERT( evt->GetSocket()->IsNonBlocking() );

					Op op = { evt->GetSocket(), OP_ASYNC_SEND,0};
					_PlanOp(op);
				}
				break;
			case SE_CLOSE:
			case SE_SHUTDOWN:
				{
					WAWO_REF_PTR<_MyListenerT> socket_l(this);
					evt->GetSocket()->UnRegister(socket_l);

					Op op = { evt->GetSocket(), OP_REMOVE, evt->GetEventData().int32_v };
					_PlanOp(op);
				}
				break;
			case SE_SHUTDOWN_RD:
				{
					WAWO_REF_PTR<_MyListenerT> socket_l(this);
					evt->GetSocket()->UnRegister(evt_id,socket_l);

					int const& ec = evt->GetEventData().int32_v;
					WAWO_LOG_WARN("socket_proxy", "[#%d:%s]socket error: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetAddr().AddressInfo(), ec );

					Op op = { evt->GetSocket(), OP_SHUTDOWN_RD, ec };
					_PlanOp(op);
				}
				break;
			case SE_SHUTDOWN_WR:
				{
					WAWO_REF_PTR<_MyListenerT> socket_l(this);
					evt->GetSocket()->UnRegister(evt_id,socket_l);

					int const& ec = evt->GetEventData().int32_v;
					WAWO_LOG_WARN("socket_proxy", "[#%d:%s]socket error: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetAddr().AddressInfo(), ec );

					Op op = { evt->GetSocket(), OP_SHUTDOWN_WR, ec };
					_PlanOp(op);
				}
				break;
			case SE_ERROR:
				{
					int const& ec = evt->GetEventData().int32_v;
					WAWO_LOG_WARN("socket_proxy", "[#%d:%s]socket error: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetAddr().AddressInfo(), ec );
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

		int AsyncConnect( WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( IsRunning() );

			Op op = {socket,OP_CONNECT,0};
			_PlanOp(op);

			return wawo::OK ;
		}


		 /* we have SocketAccepted & SocketConnected specifically for passive and active socket*/
		int AddSocket(WAWO_REF_PTR<MySocketT> const& socket) {
			WAWO_ASSERT( m_state == S_RUN );
			WAWO_ASSERT( m_socket_observer != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() && !socket->IsIdle() );
			Op op = { socket, OP_ADD, 0 };
			_PlanOp(op);

			return wawo::OK ;
		}

		int RemoveSocket(WAWO_REF_PTR<MySocketT> const& socket) {
			WAWO_ASSERT( m_state == S_RUN );
			WAWO_ASSERT( !socket->IsIdle() );

			WAWO_REF_PTR<_MyListenerT> socket_l(this);
			socket->UnRegister( socket_l );

			Op op = { socket, OP_REMOVE,0 };
			_PlanOp(op);

			return wawo::OK ;
		}

		int StartListen( WAWO_REF_PTR<MySocketT> const& socket ) {

			WAWO_ASSERT( m_state == S_RUN );
			WAWO_ASSERT( m_socket_observer != NULL );

			Op op = { socket, OP_START_LISTEN,0 };
			_PlanOp(op);

			return wawo::OK ;
		}

		int StopListen( WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( m_state == S_RUN );

			Op op = { socket, OP_STOP_LISTEN, 0 };
			_PlanOp(op);
			return wawo::OK;
		}
	};

}}}
#endif
