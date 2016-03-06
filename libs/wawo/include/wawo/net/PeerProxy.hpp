#ifndef _WAWO_NET_PEER_PROXY_HPP_
#define _WAWO_NET_PEER_PROXY_HPP_


#include <map>

#include <wawo/core.h>
#include <wawo/net/core/Event.hpp>

#include <wawo/net/core/SocketProxy.hpp>
#include <wawo/net/ServiceProvider_Abstract.hpp>
#include <wawo/net/Peer_Abstract.hpp>

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/core/Dispatcher_Abstract.hpp>


/*
 * Peer proxy is used to manage peers that accepted from remote connect , or the peers that we connect out actively.
 * Peer proxy is also a holder of all service providers
 */

namespace wawo { namespace net {

	using namespace wawo::thread;
	using namespace wawo::net::core;

	template <class _BasePeerT>
	class PeerProxy_Abstract :
		virtual public RefObject_Abstract
	{
	public:
		virtual void HandleError( WAWO_REF_PTR<_BasePeerT> const& peer, int const& ec ) =0;
		virtual void HandleDisconnected( WAWO_REF_PTR<_BasePeerT> const& peer, int const& ec ) = 0;
	};

	template <class _MyPeerT = wawo::net::peer::Wawo<> >
	class PeerProxy :
		public Listener_Abstract< typename _MyPeerT::MySocketEventT >,
		public Dispatcher_Abstract< PeerEvent<_MyPeerT> >,
		public PeerProxy_Abstract< typename _MyPeerT::MyBasePeerT>,
		public ThreadRunObject_Abstract
	{
		enum _RunState {
			S_IDLE,
			S_RUN,
			S_EXIT
		};
	public:
		typedef _MyPeerT MyPeerT;

		typedef typename MyPeerT::MyBasePeerT MyBasePeerT;
		typedef typename MyPeerT::MyCredentialT MyCredentialT;
		typedef typename MyPeerT::MyMessageT MyMessageT;
		typedef typename MyPeerT::MySocketT MySocketT;
		typedef typename MySocketT::SocketEventT MySocketEventT;

		typedef PeerEvent<_MyPeerT> MyPeerEventT;
		typedef Dispatcher_Abstract< MyPeerEventT > _MyPeerEventDispatcherT;

		typedef Listener_Abstract< typename MyPeerT::MySocketEventT > _MySocketEventListenerT;

		typedef typename MyPeerT::MyPeerProxyT MyPeerProxyT;

		typedef SocketProxy<typename MyPeerT::MySocketT> MySocketProxyT ;

		typedef std::vector< WAWO_REF_PTR<MyBasePeerT> > PeerPool;

		typedef std::map<SocketAddr, WAWO_REF_PTR<MySocketT> > ListenSocketPairs;
		typedef std::pair<SocketAddr, WAWO_REF_PTR<MySocketT> > SocketPair;

		struct ConnectingPeerInfo {
			WAWO_REF_PTR<MyBasePeerT> peer;
			WAWO_REF_PTR<MySocketT> socket;
		};
		typedef std::vector< ConnectingPeerInfo > ConnectingPeerPool;

		enum PeerOpCode {
			OP_ACCEPTED,
			OP_CONNECT,
			OP_CONNECTED,
			OP_DISCONNECT,
			OP_STOP_LISTEN, //for listener
			OP_DISCONNECT_AND_REMOVE,
			OP_ADD,
			OP_REMOVE,
			OP_ERROR
		};

		struct PeerOp {
			WAWO_REF_PTR<MyBasePeerT> peer;
			WAWO_REF_PTR<MySocketT> socket;
			PeerOpCode code:8;
			int ec:24;			//for connect

			explicit PeerOp( PeerOpCode const& op,WAWO_REF_PTR<MyBasePeerT> const& peer, int const& ec=0 ):
				peer(peer),
					socket(),
					code(op),
					ec(ec)
			{
			}

			explicit PeerOp( PeerOpCode const& op,WAWO_REF_PTR<MyBasePeerT> const& peer, WAWO_REF_PTR<MySocketT> const& socket, int const& ec=0):
			peer(peer),
			socket(socket),
				code(op),
				ec(ec)
			{
				WAWO_ASSERT( socket != NULL );
				WAWO_ASSERT( !socket->GetAddr().IsNullAddr() );
			}
		};

		typedef std::queue<PeerOp> PeerOpQueue;

	private:
		SharedMutex m_mutex;
		int m_state;

		SpinMutex m_ops_mutex;
		PeerOpQueue m_ops;

		//SpinMutex m_clients_mutex;
		PeerPool m_peers;

		//SpinMutex m_connecting_clients_mutex;
		ConnectingPeerPool m_connecting_infos;

		SpinMutex m_listen_socket_mutex;
		ListenSocketPairs m_listen_sockets;

		WAWO_REF_PTR<MySocketProxyT> m_socket_proxy ;

#ifdef WAWO_ENABLE_SERVICE_EXECUTOR
		wawo::task::TaskManager* m_task_executor;
#endif

		wawo::net::core::SockBufferConfig m_global_socket_buffer_cfgs;
	private:

		void _Init() {
			WAWO_ASSERT( m_socket_proxy == NULL );
			m_socket_proxy = WAWO_REF_PTR<MySocketProxyT> (new MySocketProxyT());
		}

		void _Deinit() {
			WAWO_ASSERT( m_socket_proxy != NULL );
			m_socket_proxy = NULL;
		}

		inline void _PlanOp(PeerOp const& op) {
			LockGuard<SpinMutex> lg_ops( m_ops_mutex );
			m_ops.push(op);
		}

		void _ExecuteOps() {

			LockGuard<SpinMutex> lg_ops( m_ops_mutex );
			while( !m_ops.empty() ) {

				PeerOp peer_op = m_ops.front();
				m_ops.pop();

				WAWO_REF_PTR<MyBasePeerT> const& peer	= peer_op.peer;
				WAWO_REF_PTR<MySocketT> const socket	= peer_op.socket;
				PeerOpCode const& code					= peer_op.code;
				int const& ec							= peer_op.ec;

				switch( code ) {

				case OP_ADD:
					{
						switch(m_state) {
						case S_RUN:
							{
								WAWO_ASSERT( peer != NULL );
								WAWO_ASSERT( peer->GetSocket() != NULL );
								WAWO_ASSERT( peer->GetSocket()->IsNonBlocking() );

								typename PeerPool::iterator it = std::find( m_peers.begin(), m_peers.end(), peer );
								WAWO_ASSERT( it == m_peers.end() );
								m_peers.push_back(peer);

								peer->AssignProxy( WAWO_REF_PTR<MyPeerProxyT>(this) );
								WAWO_ASSERT( m_socket_proxy != NULL );
								m_socket_proxy->AddSocket(peer->GetSocket());
							}
							break;
						case S_IDLE:
						case S_EXIT:
							{
								WAWO_REF_PTR<MySocketT> socket = peer->GetSocket();
								peer->DetachSocket();
								socket->Close();
							}
							break;
						}
					}
					break;
				case OP_REMOVE:
					{
						WAWO_ASSERT( peer != NULL );
						WAWO_ASSERT( peer->GetSocket() == NULL );

						typename PeerPool::iterator it = std::find( m_peers.begin(), m_peers.end(), peer );
						if( it != m_peers.end() ) {
							m_peers.erase(it);
						}
					}
					break;
				case OP_ACCEPTED:
					{
						WAWO_ASSERT( peer == NULL );
						WAWO_ASSERT( socket != NULL );
						WAWO_ASSERT( socket->IsPassive() );

						MyCredentialT credential( Len_CStr("anonymous"), Len_CStr(""));
						WAWO_ASSERT(socket->IsNonBlocking());

						WAWO_REF_PTR<MyBasePeerT> _peer( new MyPeerT( credential ) );
						_peer->AttachSocket(socket);

						WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT(PE_CONNECTED, wawo::static_pointer_cast<MyPeerT>(_peer) ) );
						_MyPeerEventDispatcherT::Raise(evt);
					}
					break;
				case OP_CONNECTED:
					{
						WAWO_ASSERT( socket != NULL );
						WAWO_ASSERT( socket->IsActive() );

						typename ConnectingPeerPool::iterator it = std::find_if( m_connecting_infos.begin(), m_connecting_infos.end(), [&] ( ConnectingPeerInfo const& info ) {
							return info.socket == socket;
						});

						WAWO_ASSERT( it != m_connecting_infos.end() );
						WAWO_ASSERT( it->peer != NULL );

						WAWO_REF_PTR<_MySocketEventListenerT> socket_l( this );
						socket->UnRegister( SE_CONNECTED, socket_l );
						socket->UnRegister( SE_ERROR, socket_l );

						it->peer->AttachSocket(socket);
						WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT(PE_CONNECTED, wawo::static_pointer_cast<MyPeerT>(it->peer) ) );
						_MyPeerEventDispatcherT::Raise(evt);
					}
					break;
				case OP_DISCONNECT:
					{
						peer->DetachSocket();

						WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT(PE_DISCONNECTED, wawo::static_pointer_cast<MyPeerT>(peer), ec) );
						_MyPeerEventDispatcherT::Raise(evt);
					}
					break;
				case OP_STOP_LISTEN:
					{
						WAWO_ASSERT( socket->IsListener() );
						StopListenOn( socket->GetAddr() );
					}
					break;
				case OP_DISCONNECT_AND_REMOVE:
					{
						WAWO_REF_PTR<MySocketT> socket = peer->GetSocket() ;

						WAWO_REF_PTR<typename MyBasePeerT::ListenerT> peer_l(peer.Get());
						socket->UnRegister( peer_l );
						socket->Flush(2000); /*WAIT FOR 2 SECONDS IF send blocks*/
						socket->Shutdown(Socket::SSHUT_RDWR);
						peer->DetachSocket();

						PeerOp op( OP_REMOVE,peer, ec);
						m_ops.push(op);
					}
					break;
				case OP_CONNECT:
					{
						switch(m_state) {
						case S_RUN:
							{
								WAWO_ASSERT( m_socket_proxy != NULL );
								WAWO_ASSERT( peer != NULL );
								WAWO_ASSERT( !socket->GetAddr().IsNullAddr() );
								WAWO_ASSERT( socket->IsIdle() );
#ifdef _DEBUG
								typename ConnectingPeerPool::iterator it = std::find_if(m_connecting_infos.begin(), m_connecting_infos.end(), [&](ConnectingPeerInfo const& info) {
									return socket == info.socket ;
								});
								WAWO_ASSERT( it == m_connecting_infos.end() );
#endif

								WAWO_REF_PTR<_MySocketEventListenerT> socket_l( this );

								socket->Register(SE_CONNECTED, socket_l );
								socket->Register(SE_ERROR, socket_l );

								ConnectingPeerInfo info = {peer, socket};
								m_connecting_infos.push_back(info);

								int rt = m_socket_proxy->AsyncConnect(socket);
								WAWO_ASSERT( rt == wawo::OK );
							}
							break;
						case S_EXIT:
							{
								WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT( PE_ERROR, wawo::static_pointer_cast<MyPeerT>(peer), ec) );
								_MyPeerEventDispatcherT::Trigger(evt);
							}
							break;
						case S_IDLE:
							{
								WAWO_ASSERT(!"WAWO,,,WHAT!!!!!!");
							}
							break;
						}
					}
					break;
				case OP_ERROR:
					{
						switch(ec) {
						case wawo::E_WSAECONNREFUSED:
						case wawo::E_WSAETIMEDOUT:
						case wawo::E_WSAEADDRINUSE:
						case wawo::E_ECONNREFUSED:
						case wawo::E_ETIMEOUT:
						case wawo::E_EADDRINUSE:
						case wawo::E_SOCKET_INVALID_STATE:
						case wawo::E_SOCKET_PROXY_INVALID_STATE:
						case wawo::E_SOCKET_CONNECTING_CANCEL:
						case wawo::E_SOCKET_PROXY_EXIT:
							{
								typename ConnectingPeerPool::iterator it = std::find_if( m_connecting_infos.begin(), m_connecting_infos.end(), [&] ( ConnectingPeerInfo const& info ) {
									return info.socket == socket;
								});

								WAWO_ASSERT( it != m_connecting_infos.end() );
								WAWO_ASSERT( it->peer != NULL );

								WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT(PE_ERROR, wawo::static_pointer_cast<MyPeerT>(it->peer) , ec) );
								_MyPeerEventDispatcherT::Trigger(evt);

								m_connecting_infos.erase(it);
								socket->Close(ec);
							}
							break;
						case wawo::E_ECONNABORTED:
						case wawo::E_ECONNRESET:
						case wawo::E_WSAECONNRESET:
						case wawo::E_WSAECONNABORTED:
						case wawo::E_EPIPE:
						case wawo::E_SOCKET_NOT_CONNECTED:
							{
								WAWO_ASSERT(!"what");

								WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT( PE_ERROR, wawo::static_pointer_cast<MyPeerT>(peer), ec) );
								_MyPeerEventDispatcherT::Raise(evt);
							}
							break;
						case wawo::E_EMFILE:
							{
								WAWO_ASSERT( socket->IsListener() );
								WAWO_REF_PTR<MyPeerEventT> evt( new MyPeerEventT(PE_ERROR, wawo::static_pointer_cast<MyPeerT>(peer), ec) );
								_MyPeerEventDispatcherT::Raise(evt);
							}
							break;
						default:
							{
								char tmp[256]={0};
								snprintf( tmp, sizeof(tmp)/sizeof(tmp[0]), "unknown peer error: %d", ec );
								WAWO_THROW_EXCEPTION( tmp );
							}
							break;
						}
					}
					break;
				}
			}
		}

	public:
		virtual void Update() {}

		//discover_addr, remote discover addr
		//socket_server_addrs, local socket server
		PeerProxy():
			m_state(S_IDLE),
			m_socket_proxy(NULL),
			m_global_socket_buffer_cfgs( BufferConfigs[SBCT_EXTREM_TINY] )
#ifdef WAWO_ENABLE_SERVICE_EXECUTOR
			,m_task_executor(NULL)
#endif
		{
			_Init();
		}

		virtual ~PeerProxy() {
			Stop();
			_ExecuteOps();
			_Deinit();
		}
		void Stop() {
			{
				LockGuard<SharedMutex> _lg(m_mutex);
				if( m_state == S_EXIT ) {return ;}
				m_state = S_EXIT;
			}
			ThreadRunObject_Abstract::Stop();
		}

		void OnStart() {
			LockGuard<SharedMutex> _lg(m_mutex);
			WAWO_ASSERT( m_state==S_IDLE );
			m_state = S_RUN;

			WAWO_ASSERT( m_socket_proxy != NULL );
			int rt = m_socket_proxy->Start();
			if( rt != wawo::OK ) {
				m_state = S_EXIT;
			}

#ifdef WAWO_ENABLE_SERVICE_EXECUTOR
			m_task_executor = new wawo::task::TaskManager( std::thread::hardware_concurrency()*2 );
			WAWO_NULL_POINT_CHECK( m_task_executor );
#endif
		}

		void OnStop() {

			LockGuard<SharedMutex> _lg( m_mutex ) ;
			WAWO_ASSERT( m_state == S_EXIT );

	#ifdef WAWO_ENABLE_SERVICE_EXECUTOR
			WAWO_DELETE( m_task_executor );
	#endif

			StopAllListen();
			_ExecuteOps();

			std::for_each( m_connecting_infos.begin(), m_connecting_infos.end(), [&]( ConnectingPeerInfo const& info ) {
				PeerOp op( OP_ERROR, info.peer, info.socket, wawo::E_SOCKET_CONNECTING_CANCEL );
				_PlanOp(op);
			});
			std::for_each( m_peers.begin(), m_peers.end(), [&]( WAWO_REF_PTR<MyBasePeerT> const& peer ) {
				PeerOp op( OP_DISCONNECT_AND_REMOVE, peer );
				_PlanOp(op);
			});
			_ExecuteOps();

			WAWO_ASSERT( m_socket_proxy != NULL );
			m_socket_proxy->Stop();
		}

		void Run() {
			{
				LockGuard<SharedMutex> lg(m_mutex);
				switch( m_state ) {
				case S_RUN:
					{
						_ExecuteOps();
						std::for_each( m_peers.begin(), m_peers.end(), [&](WAWO_REF_PTR<MyBasePeerT> const& peer ) {
							peer->Tick();
						});

						Update();
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
				}
			}

			if( m_ops.size() == 0 ) {
				wawo::usleep(32);
			}
		}

		void AddPeer( WAWO_REF_PTR<MyPeerT> const& peer ) {
			PeerOp op( OP_ADD, wawo::static_pointer_cast<MyBasePeerT>(peer) );
			_PlanOp(op);
		}

		void RemovePeer( WAWO_REF_PTR<MyPeerT> const& peer ) {
			PeerOp op( OP_REMOVE, wawo::static_pointer_cast<MyBasePeerT>(peer) );
			_PlanOp(op);
		}

		int StartListenOn( wawo::net::core::SocketAddr const& addr ) {
			WAWO_ASSERT( m_listen_sockets.find( addr ) == m_listen_sockets.end() );

			WAWO_REF_PTR<MySocketT> listen_socket (new MySocketT( m_global_socket_buffer_cfgs, wawo::net::core::Socket::TYPE_STREAM, wawo::net::core::Socket::PROTOCOL_TCP,wawo::net::core::Socket::FAMILY_AF_INET ) );

			int open = listen_socket->Open();
			if( open != wawo::OK ) {
				listen_socket->Close(open);
				return open;
			}

			int bind = listen_socket->Bind( addr );
			if(bind != wawo::OK) {
				listen_socket->Close(bind);
				return bind;
			}

			int turn_on_nonblocking = listen_socket->TurnOnNonBlocking();

			if( turn_on_nonblocking != wawo::OK ) {
				listen_socket->Close(turn_on_nonblocking);
				return turn_on_nonblocking;
			}

			listen_socket->Register( SE_ACCEPTED, WAWO_REF_PTR<_MySocketEventListenerT>(this) );
			listen_socket->Register( SE_ERROR, WAWO_REF_PTR<_MySocketEventListenerT>(this));
			listen_socket->Register( SE_CLOSE, WAWO_REF_PTR<_MySocketEventListenerT>(this) , true);

			LockGuard <SpinMutex> lg(m_listen_socket_mutex);
			m_listen_sockets.insert( SocketPair(addr, listen_socket) );

			return m_socket_proxy->StartListen( listen_socket ) ;
		}

		int StopListenOn(wawo::net::core::SocketAddr const& addr) {
			WAWO_ASSERT( m_socket_proxy != NULL );

			WAWO_ASSERT( m_listen_sockets.find( addr ) != m_listen_sockets.end() );

			typename ListenSocketPairs::iterator it = m_listen_sockets.find(addr);
			WAWO_ASSERT( it != m_listen_sockets.end() );

			WAWO_REF_PTR<MySocketT> socket = it->second;
			WAWO_ASSERT( socket != NULL );

			socket->UnRegister( WAWO_REF_PTR<_MySocketEventListenerT>(this) ,true);
			m_listen_sockets.erase(it);

			return m_socket_proxy->StopListen(socket);
		}

		void StopAllListen() {

			std::for_each( m_listen_sockets.begin(), m_listen_sockets.end(), [&](SocketPair const& pair) {
				WAWO_REF_PTR<_MySocketEventListenerT> socket_l(this);
				pair.second->UnRegister( socket_l );
				m_socket_proxy->StopListen(pair.second);
			});

			m_listen_sockets.clear();
		}

		WAWO_REF_PTR<MySocketProxyT> const& GetSocketProxy() {
			return m_socket_proxy ;
		}

		void AsyncConnect( WAWO_REF_PTR<MyPeerT> const& peer, WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( peer != NULL );

			PeerOp op( OP_CONNECT, wawo::static_pointer_cast<MyBasePeerT>(peer), socket );
			_PlanOp(op);
		}

		void HandleDisconnected( WAWO_REF_PTR<MyBasePeerT> const& peer, int const& ec ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if( m_state != S_RUN ) {
				return ;
			}

			WAWO_ASSERT( peer != NULL );
			PeerOp op ( OP_DISCONNECT, peer,ec );
			_PlanOp(op);
		}

		void HandleError( WAWO_REF_PTR<MyBasePeerT> const& peer, int const& ec ) {
			SharedLockGuard<SharedMutex> lg(m_mutex);
			if( m_state != S_RUN ) {
				return ;
			}

			WAWO_ASSERT( peer != NULL );

			PeerOp op ( OP_ERROR, peer, ec );
			_PlanOp(op);
		}

		void OnEvent( WAWO_REF_PTR<MySocketEventT> const& evt ) {

			SharedLockGuard<SharedMutex> _lg(m_mutex);
			switch( m_state ) {

			case S_RUN:
				{
					int id = evt->GetId();
					WAWO_ASSERT( evt->GetSocket() != NULL );

					switch( id ) {
					case wawo::net::core::SE_ACCEPTED:
						{
							WAWO_ASSERT( evt->GetSocket()->IsListener() ) ;
							MySocketT* socket_ptr = (MySocketT*) (evt->GetEventData().ptr_v);
							//must be passive
							WAWO_REF_PTR<MySocketT> socket ( socket_ptr ) ;
							WAWO_ASSERT( socket->IsPassive() );

							int rt = socket->TurnOnNonBlocking();
							if( rt == wawo::OK ) {
								PeerOp op( OP_ACCEPTED, WAWO_REF_PTR<MyBasePeerT>(NULL), socket ) ;
								_PlanOp(op);
							} else {
								WAWO_LOG_WARN("client_proxy", "[%d:%s] new socket connected, but turnon nonblocking failed, error code: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), rt );
								socket->Shutdown(Socket::SSHUT_RDWR, rt);
							}
						}
						break;
					case SE_CONNECTED:
						{
							WAWO_ASSERT( evt->GetSocket()->IsActive() );
							PeerOp op ( OP_CONNECTED, WAWO_REF_PTR<MyBasePeerT>(NULL) , evt->GetSocket() );
							_PlanOp(op);
						}
						break;
					case SE_ERROR:
						{
							PeerOp op (OP_ERROR, WAWO_REF_PTR<MyBasePeerT>(NULL), evt->GetSocket(), evt->GetEventData().int32_v );
							_PlanOp(op);
						}
						break;
					case SE_CLOSE:
						{
							WAWO_ASSERT( evt->GetSocket()->IsListener() );
							PeerOp op (OP_STOP_LISTEN, WAWO_REF_PTR<MyBasePeerT>(NULL), evt->GetSocket(), evt->GetEventData().int32_v );
							_PlanOp(op);
						}
						break;
					default:
						{
							WAWO_THROW_EXCEPTION("unknown socket evt type");
						}
						break;
					}
				}
				break;
			case S_IDLE:
			case S_EXIT:
				{
					evt->GetSocket()->Close(Socket::SSHUT_RDWR);
				}
				break;
			}
		}

		void SetGlobalSocketBufferConfigs( wawo::net::core::SockBufferConfig const& buffer_settings ) {
			m_global_socket_buffer_cfgs = buffer_settings ;
		}
		wawo::net::core::SockBufferConfig& GetGlobalSocketBufferConfigs() {
			return m_global_socket_buffer_cfgs;
		}
	};
}}

namespace wawo { namespace net {
	typedef wawo::net::PeerProxy<> WawoPeerProxy;
}}
#endif
