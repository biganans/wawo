#ifndef _WAWO_NET_PEER_PROXY_HPP_
#define _WAWO_NET_PEER_PROXY_HPP_


#include <map>

#include <wawo/core.h>
#include <wawo/net/NetEvent.hpp>
#include <wawo/net/SocketProxy.hpp>

#include <wawo/net/core/Listener_Abstract.hpp>

#define WAWO_LISTEN_BACK_LOG 512

/*
* Peer proxy is used to manage peers that accepted from remote connect , and the peers that we connect to remote actively.
*/

namespace wawo { namespace net {

	using namespace wawo::thread;
	using namespace wawo::net::core;

	template <class _PeerT = wawo::net::peer::Wawo<> >
	class PeerProxy :
		public ThreadRunObject_Abstract,
		public Listener_Abstract< SocketEvent >,
		public Listener_Abstract< PeerEvent<_PeerT> >
	{
	public:
		typedef _PeerT PeerT;
		typedef PeerProxy<_PeerT> MyT;

		typedef PeerEvent<_PeerT> PeerEventT;

		typedef Listener_Abstract< SocketEvent > SEvtListenerT;
		typedef Listener_Abstract< PeerEvent<_PeerT> > PEvtListenerT;
	private:

		typedef std::vector< WWRP<PeerT> > PeerPool;

		typedef std::map<SocketAddr, WWRP<Socket> > ListenSocketPairs;
		typedef std::pair<SocketAddr, WWRP<Socket> > SocketPair;

		struct ConnectingPeerInfo {
			WWRP<PeerT> peer;
			WWRP<Socket> socket;
		};

		typedef std::vector<ConnectingPeerInfo> ConnectingPeerPool;

		enum _RunState {
			S_IDLE,
			S_RUN,
			S_EXIT
		};

		enum PeerOpCode {
			OP_NONE,
			OP_SOCKET_CONNECTED,
			OP_SOCKET_ERROR,
			OP_SOCKET_CLOSE,

			OP_PEER_SOCKET_CLOSE,
			OP_PEER_CLOSE,
			OP_PEER_CLOSE_AND_REMOVE,

			//OP_ADD,
			OP_PEER_REMOVE,
		};

		struct PeerOp {
			WWRP<PeerT> peer;
			WWRP<Socket> socket;
			PeerOpCode code : 8;
			int ec : 24;			//for connect

			explicit PeerOp() :
				peer(NULL),
				socket(NULL),
				code(OP_NONE),
				ec(0)
			{}
			explicit PeerOp(PeerOpCode const& op, WWRP<PeerT> const& peer, WWRP<Socket> const& socket, int const& ec = 0) :
				peer(peer),
				socket(socket),
				code(op),
				ec(ec)
			{
			}
		};

		typedef std::queue<PeerOp> PeerOpQueue;

	private:
		SharedMutex m_mutex;
		int m_state;

		WWRP<PEvtListenerT> m_pevt_listener;
		WWRP<SocketProxy> m_socket_proxy;

		SpinMutex m_ops_mutex;
		PeerOpQueue m_ops;

		SpinMutex m_peers_mutex;
		PeerPool m_peers;

		SpinMutex m_connecting_infos_mutex;
		ConnectingPeerPool m_connecting_infos;

		SpinMutex m_listen_sockets_mutex;
		ListenSocketPairs m_listen_sockets;
	private:
		inline void _PlanOp(PeerOp const& op) {
			LockGuard<SpinMutex> lg_ops(m_ops_mutex);
			m_ops.push(op);
		}
		inline void _PopOp(PeerOp& op) {
			LockGuard<SpinMutex> lg_ops(m_ops_mutex);
			if (m_ops.empty()) return;
			op = m_ops.front();
			m_ops.pop();
		}

		inline void _ExecuteOps() {

			while (!m_ops.empty()) {
				PeerOp pop;
				_PopOp(pop);
				if (pop.code == OP_NONE) {
					break;
				}

				WWRP<PeerT> const& peer = pop.peer;
				WWRP<Socket> const& socket = pop.socket;
				PeerOpCode const& code = pop.code;
				int const& ec = pop.ec;

				switch (code) {
				case OP_SOCKET_CONNECTED:
				{
					if (m_state != S_RUN) {
						socket->Close(wawo::E_PEER_PROXY_INVALID_STATE);
					} else {
						WAWO_ASSERT(socket != NULL);
						WAWO_ASSERT(socket->IsActive());
						WAWO_ASSERT(m_pevt_listener != NULL);

						LockGuard<SpinMutex> lg(m_connecting_infos_mutex);
						typename ConnectingPeerPool::iterator it = std::find_if(m_connecting_infos.begin(), m_connecting_infos.end(), [&socket](ConnectingPeerInfo const& info) {
							return info.socket == socket;
						});
						WAWO_ASSERT(it != m_connecting_infos.end());
						WAWO_ASSERT(it->peer != NULL);
						WAWO_ASSERT(it->socket == socket);

						WWRP<SEvtListenerT> socket_l(this);
						socket->UnRegister(socket_l);
						m_connecting_infos.erase(it);
					}
				}
				break;
				case OP_SOCKET_ERROR:
				{
					switch (ec) {
					case wawo::E_WSAECONNREFUSED:
					case wawo::E_WSAETIMEDOUT:
					case wawo::E_WSAEADDRINUSE:
					case wawo::E_ENETUNREACH:
					case wawo::E_EHOSTUNREACH:
					case wawo::E_ECONNREFUSED:
					case wawo::E_ETIMEOUT:
					case wawo::E_EADDRINUSE:
					case wawo::E_SOCKET_INVALID_STATE:
					case wawo::E_SOCKET_CONNECTING_CANCEL:
					case wawo::E_SOCKET_PROXY_EXIT:
					case wawo::E_SOCKET_RECEIVED_RST:
					case wawo::E_SOCKET_FORCE_CLOSE:
					{
						LockGuard<SpinMutex> lg(m_connecting_infos_mutex);
						typename ConnectingPeerPool::iterator it = std::find_if(m_connecting_infos.begin(), m_connecting_infos.end(), [&socket](ConnectingPeerInfo const& info) {
							return info.socket == socket;
						});
						WAWO_ASSERT(it != m_connecting_infos.end());
						m_connecting_infos.erase(it);

						WAWO_WARN("[peer_proxy][#%d:%s]socket connect failed, close, ec: %d", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), ec);
					}
					break;
					case wawo::E_EMFILE:
					{
						WAWO_ASSERT(peer == NULL);
						WAWO_ASSERT(socket->IsListener());
						WAWO_THROW_EXCEPTION("max socket fds reach");

						//WWRP<PeerEventT> evt( new PeerEventT(PE_ERROR, peer, socket, ec) );
						//u64_t otag = reinterpret_cast<u64_t>(socket.Get());
						//_DispatcherT::OSchedule(evt,otag);
					}
					break;
					default:
					{
						char tmp[256] = { 0 };
						snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown socket error: %d", ec);
						WAWO_THROW_EXCEPTION(tmp);
					}
					break;
					}
				}
				break;
				case OP_SOCKET_CLOSE:
				{
					WAWO_ASSERT(socket->IsClosed());
					if ( socket->IsListener() ) {
						LockGuard <SpinMutex> lg(m_listen_sockets_mutex);
						typename ListenSocketPairs::iterator it = m_listen_sockets.find(socket->GetRemoteAddr());
						WAWO_ASSERT(it != m_listen_sockets.end());
						WWRP<Socket> socket = it->second;
						WAWO_ASSERT(socket != NULL);
						m_listen_sockets.erase(it);
					}
					else {

						WAWO_ASSERT( !"what" );
						WAWO_ASSERT(socket->IsActive());

						LockGuard<SpinMutex> lg(m_connecting_infos_mutex);
						typename ConnectingPeerPool::iterator it = std::find_if(m_connecting_infos.begin(), m_connecting_infos.end(), [&socket](ConnectingPeerInfo const& info) {
							return info.socket == socket;
						});

						WAWO_ASSERT(it != m_connecting_infos.end());
						WAWO_ASSERT(it->peer != NULL);

						WWRP<SEvtListenerT> socket_l(this);
						socket->UnRegister(socket_l);

						m_connecting_infos.erase(it);
					}
				}
				break;
				case OP_PEER_REMOVE:
				{
					_RemovePeer(peer);
				}
				break;
				case OP_PEER_CLOSE:
				{
					PeerOp op_remove(OP_PEER_REMOVE, peer, socket, ec);
					_PlanOp(op_remove);
				}
				break;
				case OP_PEER_CLOSE_AND_REMOVE:
				{
					WAWO_ASSERT(m_state == S_EXIT);
					WAWO_ASSERT(peer != NULL);
					std::vector< WWRP<Socket> > sockets;
					peer->GetSockets(sockets);

					std::for_each(sockets.begin(), sockets.end(), [&peer, this, &ec](WWRP<Socket> const& socket) {
						peer->DetachSocket(socket);
						socket->Close(wawo::E_PEER_PROXY_EXIT);
					});

					PeerOp op_remove(OP_PEER_REMOVE, peer, NULL, ec);
					_PlanOp(op_remove);
				}
				break;
				case OP_NONE:
				{//just for warning
				}
				break;
				default:
				{
					char tmp[256] = { 0 };
					snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown peer opcode : %d", code);
					WAWO_THROW_EXCEPTION(tmp);
				}
				break;
				}
			}
		}

	public:
		PeerProxy(WWRP<PEvtListenerT> const& l) :
			m_state(S_IDLE),
			m_pevt_listener(l),
			m_socket_proxy(NULL)
		{
		}

		virtual ~PeerProxy() {
		}

		//Stop must be called explicitily
		void Stop() {
			{
				LockGuard<SharedMutex> _lg(m_mutex);
				if (m_state == S_EXIT) { return; }
				m_state = S_EXIT;
			}
			ThreadRunObject_Abstract::Stop();
		}

		void OnStart() {
			LockGuard<SharedMutex> _lg(m_mutex);
			WAWO_ASSERT(m_state == S_IDLE);
			m_state = S_RUN;

			WAWO_ASSERT(m_socket_proxy == NULL);
			m_socket_proxy = WWRP<SocketProxy>(new SocketProxy());
			WAWO_NULL_POINT_CHECK(m_socket_proxy);


			WAWO_ASSERT(m_socket_proxy != NULL);
			int rt = m_socket_proxy->Start();
			if (rt != wawo::OK) {
				m_state = S_EXIT;
			}
		}

		void OnStop() {

			SharedLockGuard<SharedMutex> _lg(m_mutex);
			WAWO_ASSERT(m_state == S_EXIT);

			StopAllListener();
			_ExecuteOps();

			{
				LockGuard<SpinMutex> lg_peers(m_connecting_infos_mutex);
				std::for_each(m_connecting_infos.begin(), m_connecting_infos.end(), [this](ConnectingPeerInfo const& info) {
					info.socket->Close(wawo::E_SOCKET_CONNECTING_CANCEL);
					PeerOp op(OP_SOCKET_CLOSE, info.peer, info.socket, wawo::E_SOCKET_CONNECTING_CANCEL);
					_PlanOp(op);
				});
			}

			{
				LockGuard<SpinMutex> lg_peers(m_peers_mutex);
				std::for_each(m_peers.begin(), m_peers.end(), [this](WWRP<PeerT> const& peer) {
					PeerOp op(OP_PEER_CLOSE_AND_REMOVE, peer, WWRP<Socket>(NULL));
					_PlanOp(op);
				});
			}
			_ExecuteOps();

			WAWO_ASSERT(m_socket_proxy != NULL);
			m_socket_proxy->Stop();
			WAWO_ASSERT(m_state == S_EXIT);
			m_socket_proxy = NULL;

			WAWO_ASSERT(m_connecting_infos.size() == 0);
			WAWO_ASSERT(m_peers.size() == 0);

			LockGuard<SpinMutex> lg(m_ops_mutex);
			WAWO_ASSERT(m_ops.empty());

			WAWO_ASSERT(m_pevt_listener != NULL);
			m_pevt_listener = NULL;
		}

		void Run() {
			{
				SharedLockGuard<SharedMutex> lg(m_mutex);
				switch (m_state) {
				case S_RUN:
				{
					_ExecuteOps();
					LockGuard<SpinMutex> lg(m_peers_mutex);
					u32_t c = m_peers.size();
					for (u32_t i = 0; i < c; i++) {
						m_peers[i]->Tick();
					}
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

			if (m_ops.size() == 0) {
				wawo::usleep(256);
			}
		}

		int WatchPeerSocket(WWRP<Socket> const& socket, int const& io_evt) {
			WAWO_ASSERT(socket != NULL);
			WAWO_ASSERT(socket->IsNonBlocking());
			WAWO_ASSERT((io_evt>0) && io_evt <= wawo::net::IOE_ALL);

			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }

			WAWO_ASSERT(m_socket_proxy != NULL);
			int addwatch_rt = m_socket_proxy->AddSocket(socket, io_evt);
			WAWO_ASSERT(addwatch_rt == wawo::OK);

			(void)addwatch_rt;
			return wawo::OK;
		}

		int UnWatchPeerSocket(WWRP<Socket> const& socket, int const& io_evt) {
			WAWO_ASSERT(socket != NULL);
			WAWO_ASSERT(socket->IsNonBlocking());
			WAWO_ASSERT((io_evt>0) && io_evt <= wawo::net::IOE_ALL);

			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }

			WAWO_ASSERT(m_socket_proxy != NULL);
			int watchrt = m_socket_proxy->UnWatchSocket(socket, io_evt);
			WAWO_ASSERT(watchrt == wawo::OK);
			return wawo::OK;
		}

		int AddPeer(WWRP<PeerT> const& peer) {
			WAWO_ASSERT(peer != NULL);

			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }

			_AddPeer(peer);
			return wawo::OK;
		}

		inline int WatchPeerEvent(WWRP<PeerT> const& peer, u32_t const& evt_id) {
			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }

			WAWO_ASSERT(peer != NULL);
			WAWO_ASSERT(m_pevt_listener != NULL);
			peer->Register(evt_id, m_pevt_listener);
			return wawo::OK;
		}

		inline int UnWatchPeerEvent(WWRP<PeerT> const& peer, u32_t const& evt_id) {
			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }

			WAWO_ASSERT(peer != NULL);
			WAWO_ASSERT(m_pevt_listener != NULL);
			peer->UnRegister(evt_id, m_pevt_listener);
			return wawo::OK;
		}

		inline int _AddPeer(WWRP<PeerT> const& peer) {
			WAWO_ASSERT(peer != NULL);
			WAWO_ASSERT(m_state == S_RUN);

#ifdef _DEBUG
			std::vector< WWRP<Socket> > sockets;
			peer->GetSockets(sockets);
			WAWO_ASSERT(sockets.size()>0);
#endif
			LockGuard<SpinMutex> lg(m_peers_mutex);
			typename PeerPool::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
			WAWO_ASSERT(it == m_peers.end());
			m_peers.push_back(peer);
			WWRP<PEvtListenerT> peer_l(this);
			peer->Register(PE_CLOSE, peer_l);
			return wawo::OK;
		}

		int RemovePeer(WWRP<PeerT> const& peer) {
			WAWO_ASSERT(peer != NULL);

			SharedLockGuard<SharedMutex> lg_mutex(m_mutex);
			if (m_state != S_RUN) { return wawo::E_PEER_PROXY_INVALID_STATE; }
			_RemovePeer(peer);
			return wawo::OK;
		}

		inline void _RemovePeer(WWRP<PeerT> const& peer) {
			WAWO_ASSERT(peer != NULL);

#ifdef _DEBUG
			std::vector< WWRP<Socket> > sockets;
			peer->GetSockets(sockets);
			WAWO_ASSERT(sockets.size() == 0);
#endif
			LockGuard<SpinMutex> lg(m_peers_mutex);
			typename PeerPool::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
			WAWO_ASSERT(it != m_peers.end());
			WWRP<PEvtListenerT> peer_l(this);
			peer->UnRegister(PE_CLOSE, peer_l);

			WAWO_ASSERT(m_pevt_listener != NULL);
			peer->UnRegister(PE_CLOSE, m_pevt_listener);
			peer->UnRegister(PE_CONNECTED, m_pevt_listener);
			peer->UnRegister(PE_MESSAGE, m_pevt_listener);
			peer->UnRegister(PE_SOCKET_RD_SHUTDOWN, m_pevt_listener);
			peer->UnRegister(PE_SOCKET_WR_SHUTDOWN, m_pevt_listener);
			peer->UnRegister(PE_SOCKET_CLOSE, m_pevt_listener);
			peer->UnRegister(PE_SOCKET_CONNECTED, m_pevt_listener);
			m_peers.erase(it);
		}

		int StartListenOn(wawo::net::SocketAddr const& laddr, SockBufferConfig const& scfg = GetBufferConfig(SBCT_MEDIUM)) {
			LockGuard <SpinMutex> lg(m_listen_sockets_mutex);

			WAWO_ASSERT(m_listen_sockets.find(laddr) == m_listen_sockets.end());

			WWRP<Socket> lsocket(new Socket(scfg, wawo::net::F_AF_INET, wawo::net::ST_STREAM, wawo::net::P_TCP));

			int open = lsocket->Open();
			if (open != wawo::OK) {
				lsocket->Close(open);
				return open;
			}

			int bind = lsocket->Bind(laddr);
			if (bind != wawo::OK) {
				lsocket->Close(bind);
				return bind;
			}

			int turn_on_nonblocking = lsocket->TurnOnNonBlocking();

			if (turn_on_nonblocking != wawo::OK) {
				lsocket->Close(turn_on_nonblocking);
				return turn_on_nonblocking;
			}

			int listen_rt = lsocket->Listen(WAWO_LISTEN_BACK_LOG);
			if (listen_rt != wawo::OK) {
				lsocket->Close(listen_rt);
				return listen_rt;
			}
			WAWO_DEBUG("[peer_proxy] listen on: %s success", lsocket->GetLocalAddr().AddressInfo().CStr());

			lsocket->Register(SE_ACCEPTED, WWRP<SEvtListenerT>(this));
			lsocket->Register(SE_ERROR, WWRP<SEvtListenerT>(this));
			lsocket->Register(SE_CLOSE, WWRP<SEvtListenerT>(this));

			m_listen_sockets.insert(SocketPair(laddr, lsocket));

			WAWO_ASSERT(m_socket_proxy != NULL);
			m_socket_proxy->WatchSocket(lsocket, wawo::net::IOE_READ);

			return wawo::OK;
		}

		int StopListenOn(wawo::net::SocketAddr const& addr) {

			LockGuard <SpinMutex> lg(m_listen_sockets_mutex);
			typename ListenSocketPairs::iterator it = m_listen_sockets.find(addr);
			WAWO_ASSERT(it != m_listen_sockets.end());

			WWRP<Socket> socket = it->second;
			WAWO_ASSERT(socket != NULL);
			socket->UnRegister(WWRP<SEvtListenerT>(this), true);

			WAWO_ASSERT(m_socket_proxy != NULL);
			m_socket_proxy->UnWatchSocket(socket, wawo::net::IOE_READ);

			m_listen_sockets.erase(it);
			int close_rt = socket->Close();

			if (close_rt != wawo::OK) {
				WAWO_FATAL("[peer_proxy]close listen socket failed: %d", close_rt);
			}

			return close_rt;
		}

		void StopAllListener() {

			std::for_each(m_listen_sockets.begin(), m_listen_sockets.end(), [this](SocketPair const& pair) {
				m_socket_proxy->UnWatchSocket(pair.second, wawo::net::IOE_ALL);

				WWRP<SEvtListenerT> socket_l(this);
				pair.second->UnRegister(socket_l);
				pair.second->Close();
			});

			m_listen_sockets.clear();
		}

		int Connect(WWRP<PeerT> const& peer, WWRP<Socket> const& socket, bool nonblocking = false) {
			return Connect(peer, socket, NULL, 0, nonblocking);
		}

		int Connect(WWRP<PeerT> const& peer, WWRP<Socket> const& socket, void const* const cookie, u32_t const& size, bool nonblocking = false) {
			WAWO_ASSERT(peer != NULL);
			WAWO_ASSERT(socket != NULL);

			SharedLockGuard<SharedMutex> lg_mtx(m_mutex);
			if (m_state != S_RUN) {
				return wawo::E_INVALID_OPERATION;
			}

			WWRP<TLP_Abstract> tlp(new typename PeerT::TLPT());
			if (cookie != NULL) {
				tlp->Handshake_AssignCookie(cookie, size);
			}
			socket->SetTLP(tlp);

			WAWO_ASSERT(m_socket_proxy != NULL);
			if (nonblocking == false) {
				return m_socket_proxy->Connect(socket, false);
			}

			LockGuard<SpinMutex> lg_connecting_mtx(m_connecting_infos_mutex);
#ifdef _DEBUG
			typename ConnectingPeerPool::iterator it = std::find_if(m_connecting_infos.begin(), m_connecting_infos.end(), [&socket](ConnectingPeerInfo const& info) {
				return socket == info.socket;
			});
			WAWO_ASSERT(it == m_connecting_infos.end());
#endif
			WWRP<SEvtListenerT> socket_l(this);

			socket->Register(SE_CONNECTED,socket_l);
			socket->Register(SE_ERROR, socket_l);

			ConnectingPeerInfo info = { peer, socket };
			peer->Register(PE_SOCKET_ERROR, m_pevt_listener);
			peer->Register(PE_SOCKET_CONNECTED, m_pevt_listener);
			peer->Register(PE_CONNECTED, m_pevt_listener);

			peer->AttachSocket(socket);
			m_connecting_infos.push_back(info);

			int rt = m_socket_proxy->Connect(socket, true);
			WAWO_ASSERT(rt == wawo::OK);

			return rt;
		}

		void OnEvent(WWRP<SocketEvent> const& evt) {

			SharedLockGuard<SharedMutex> slg(m_mutex);
			if (m_state != S_RUN) {
				evt->GetSocket()->Close(wawo::E_SOCKET_PROXY_EXIT);
				return;
			}

			u32_t const& id = evt->GetId();
			WAWO_ASSERT(evt->GetSocket() != NULL);

			switch (id) {
			case SE_ACCEPTED:
			{
				//must be passive
				WAWO_ASSERT(evt->GetSocket()->IsListener());
				Socket* socket_ptr = (Socket*)(evt->GetCookie().ptr_v);
				WWRP<Socket> socket(socket_ptr);

				wawo::net::SocketAddr local_addr = socket->GetLocalAddr();
				WAWO_DEBUG("[peer_proxy]new socket connected, local addr: %s, remote addr: %s", socket->GetLocalAddr().AddressInfo().CStr(), socket->GetRemoteAddr().AddressInfo().CStr());

				WAWO_ASSERT(socket->IsPassive());

				WWRP<TLP_Abstract> tlp(new typename PeerT::TLPT());
				socket->SetTLP(tlp);

				int rt = socket->TurnOnNonBlocking();
				if (rt == wawo::OK) {
					WAWO_ASSERT(socket->IsPassive());
					WAWO_ASSERT(socket->IsNonBlocking());
					WAWO_ASSERT(m_pevt_listener != NULL);

					typename PeerT::LambdaFnT lambdaFNR = [socket,evt_l=m_pevt_listener]() -> void {
						WWRP<PeerT> accepted_peer(new PeerT());
						accepted_peer->AttachSocket(socket);
						accepted_peer->Register(PE_SOCKET_CONNECTED, evt_l);
						accepted_peer->Register(PE_CONNECTED, evt_l);
						int ec_o;
						WWRP<SocketEvent> sevt(new SocketEvent(socket, SE_CONNECTED));
						socket->HandlePassiveConnected(ec_o);
						if (ec_o == wawo::OK) {
							socket->Trigger(sevt);
						} else {
							accepted_peer->DetachSocket(socket);
							accepted_peer->UnRegister(PE_SOCKET_CONNECTED, evt_l);
							accepted_peer->UnRegister(PE_CONNECTED, evt_l);
						}
					};
					socket->OSchedule(lambdaFNR, nullptr);
				} else {
					WAWO_WARN("[peer_proxy][%d:%s] new socket connected, but turn on nonblocking failed, error code: %d", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), rt);
					socket->Close(rt);
				}
			}
			break;
			case SE_CONNECTED:
			{ //must be active
				WAWO_ASSERT(evt->GetSocket()->IsActive());
				PeerOp op(OP_SOCKET_CONNECTED, WWRP<PeerT>(NULL), evt->GetSocket());
				_PlanOp(op);
			}
			break;
			case SE_ERROR:
			{
				WAWO_ASSERT(evt->GetSocket()->IsActive() || evt->GetSocket()->IsListener());
				PeerOp op(OP_SOCKET_ERROR, WWRP<PeerT>(NULL), evt->GetSocket(), evt->GetCookie().int32_v);
				_PlanOp(op);
			}
			break;
			case SE_CLOSE:
			{
				PeerOp op(OP_SOCKET_CLOSE, WWRP<PeerT>(NULL), evt->GetSocket(), evt->GetCookie().int32_v);
				_PlanOp(op);
			}
			break;
			default:
			{
				char tmp[256] = { 0 };
				snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]), "unknown socket evt: %d", id);
				WAWO_THROW_EXCEPTION(tmp);
			}
			break;
			}
		}

		void OnEvent(WWRP<PeerEventT> const& evt) {
			SharedLockGuard<SharedMutex> slg(m_mutex);
			if (m_state != S_RUN) {
				return;
			}

			u32_t const& eid = evt->GetId();
			switch (eid) {
			case PE_CLOSE:
			{
				PeerOp op(OP_PEER_CLOSE, evt->GetPeer(), evt->GetSocket(), evt->GetCookie().int32_v);
				_PlanOp(op);
			}
			break;
			default:
			{
				char _tbuffer[256] = { 0 };
				snprintf(_tbuffer, sizeof(_tbuffer) / sizeof(_tbuffer[0]), "[#%d:%s]invalid peer event, eid: %d", evt->GetSocket()->GetFd(), evt->GetSocket()->GetRemoteAddr().AddressInfo().CStr(), eid);
				WAWO_THROW_EXCEPTION(_tbuffer);
			}
			break;
			}
		}
	};
}}

#endif