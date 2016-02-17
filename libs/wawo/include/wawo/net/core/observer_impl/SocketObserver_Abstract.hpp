#ifndef _WAWO_NET_CORE_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_
#define _WAWO_NET_CORE_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_

#include <wawo/SmartPtr.hpp>
#include <wawo/net/core/NetEvent.hpp>

namespace wawo { namespace net { namespace core {
	template <class _SocketT>
	class SocketObserver;
}}}


namespace wawo { namespace net { namespace core { namespace observer_impl {

	using namespace wawo::net;

	template <class _SocketT>
	class SocketObserver_Abstract:
		public wawo::RefObject_Abstract
	{

	public:
		typedef _SocketT MySocketT;
		typedef std::vector< WAWO_REF_PTR<MySocketT> > SocketPool;
		typedef SocketObserver<MySocketT> MyObserverT;

		typedef SocketAndEventConfigPair<_SocketT> MySocketAndEventConfigPair;
		typedef std::vector<MySocketAndEventConfigPair> SocketEventPairPool ;

	protected:
		MyObserverT* m_observer;
		SocketEventPairPool m_socket_custom_evt;

	public:
		inline void CheckCustomIO() {
			if( m_socket_custom_evt.size() > 0 ) {
				typename SocketEventPairPool::iterator it = m_socket_custom_evt.begin();
				while( it != m_socket_custom_evt.end() ) {
					WAWO_REF_PTR<MySocketT> const& socket = it->socket;

					if( socket->IsClosed() ) {
						it = m_socket_custom_evt.erase( it );
						continue ;
					} else {

						if( (it->flag&SocketObserverEvent::EVT_DELAY_READ_BY_OBSERVER) && (socket->GetRState() == S_READ_IDLE) ) {
							_ProcessEvent( socket, SocketObserverEvent::EVT_DELAY_READ_BY_OBSERVER);
						} else if( (it->flag&SocketObserverEvent::EVT_DELAY_WRITE_BY_OBSERVER) && (socket->GetWState() == S_WRITE_IDLE) ) {
							_ProcessEvent( socket, SocketObserverEvent::EVT_DELAY_WRITE_BY_OBSERVER);
						} else {
							//WAWO_ASSERT( !"unknown socket check flag");
						}

						++it;
					}
				}
			}
		}

		inline void _RegisterCustomIO(uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket) {
			RegisterIOEvent( m_socket_custom_evt, flag, socket );
		}

		inline void _UnRegisterCustomIO(uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket) {
			UnRegisterIOEvent( m_socket_custom_evt, flag, socket );
		}

		inline void _ProcessEvent( WAWO_REF_PTR<MySocketT> const& socket, wawo::uint16_t flag, wawo::uint16_t const& info = 0 ) {

			if( flag&SocketObserverEvent::EVT_RD ) {

				//WAWO_LOG_DEBUG("SocketObserver_Abstract","[#%d:%s] socket read state: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), socket->GetRState() );

				#ifdef WAWO_IO_MODE_SELECT
				WAWO_ASSERT( socket->GetRState() == S_READ_IDLE );
				#endif

				UniqueLock<SpinMutex> lg( socket->GetLock(Socket::L_READ), wawo::thread::try_to_lock );
				bool lck_ok = lg.own_lock() ;

				//for remote send,,posted, remote send and arrive before local read case
				if( !lck_ok || socket->GetRState() != S_READ_IDLE ) {
					//WAWO_LOG_DEBUG("SocketObserver_Abstract", "[#%d:%s] socket lock read failed, delay read state: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), socket->GetRState() );

					if( (flag&SocketObserverEvent::EVT_READ) && !socket->IsReadClosed() ) {
						m_observer->AddRegisterOp( SocketObserverEvent::EVT_DELAY_READ_BY_OBSERVER, socket );
						m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_READ, socket );
					}
				} else {

					WAWO_ASSERT( socket->GetRState() == S_READ_IDLE);
					socket->SetRState( S_READ_POSTED ) ;
					if( socket->IsListener() ) {
						m_observer->PostIOEvent( socket, IE_CAN_ACCEPT );
					} else {
						WAWO_ASSERT( socket->IsDataSocket());
						m_observer->PostIOEvent( socket, IE_CAN_READ );
					}

					if( flag&SocketObserverEvent::EVT_DELAY_READ_BY_OBSERVER ) {
						m_observer->AddRegisterOp( SocketObserverEvent::EVT_READ, socket );
						m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_DELAY_READ_BY_OBSERVER, socket );
					}
				}

				flag &= ~SocketObserverEvent::EVT_RD;
			}

			if( flag & SocketObserverEvent::EVT_WR ) {
				WAWO_ASSERT( socket->IsDataSocket() );

				UniqueLock<SpinMutex> lg( socket->GetLock(Socket::L_WRITE), wawo::thread::try_to_lock );
				bool lck_ok = lg.own_lock() ;

				WAWO_LOG_DEBUG("SocketObserver_Abstract", "[#%d:%s] socket write state: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), socket->GetWState() );

				if( !lck_ok || (socket->GetWState() != S_WRITE_IDLE)) {
					//
					WAWO_LOG_DEBUG("SocketObserver_Abstract", "[#%d:%s] socket lock write failed, delay write state: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), socket->GetRState() );

					if( (flag&SocketObserverEvent::EVT_WRITE) && !socket->IsWriteClosed() ) {
						m_observer->AddRegisterOp( SocketObserverEvent::EVT_DELAY_WRITE_BY_OBSERVER, socket );
						m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_WRITE, socket );
					}
				} else {

					WAWO_ASSERT( socket->GetWState() == S_WRITE_IDLE );

					if( socket->IsConnecting() ) {
						m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_WRITE, socket );
						socket->SetWState( S_WRITE_POSTED ) ;
						m_observer->PostIOEvent(socket, IE_CONNECTED);
					} else {

						if(socket->GetWBuffer()->BytesCount() == 0) {
							m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_WR, socket );
						}
						socket->SetWState( S_WRITE_POSTED ) ;
						m_observer->PostIOEvent(socket, IE_CAN_WRITE);
					}

					if( flag&SocketObserverEvent::EVT_DELAY_WRITE_BY_OBSERVER ) {
						m_observer->AddRegisterOp( SocketObserverEvent::EVT_WRITE, socket );
						m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_DELAY_WRITE_BY_OBSERVER, socket );
					}
				}

				flag &= ~SocketObserverEvent::EVT_WR;
			}

#ifdef WAWO_IO_MODE_EPOLL_USE_ET
			if( flag&SocketObserverEvent::EVT_CLOSE ) {
				UniqueLock<SpinMutex> lg( socket->GetLock(Socket::L_SOCKET) );

				WAWO_ASSERT( socket->GetMode() == Socket::SM_ACTIVE ||
							 socket->GetMode() == Socket::SM_PASSIVE
				);

				m_observer->PostIOEvent( WAWO_REF_PTR<MySocketT>(socket), IE_CLOSE, wawo::E_SOCKET_REMOTE_GRACE_CLOSE );
				flag &= ~SocketObserverEvent::EVT_CLOSE ;
			}
#endif
			if(flag&SocketObserverEvent::EVT_ERROR) {
				UniqueLock<SpinMutex> lg( socket->GetLock(Socket::L_SOCKET) );

				int ec;
#ifdef WAWO_PLATFORM_POSIX
				socklen_t opt_len = sizeof(int);
#elif defined WAWO_PLATFORM_WIN
				int opt_len = sizeof(int);
#else
	#error
#endif
				if (getsockopt( socket->GetFd(), SOL_SOCKET, SO_ERROR, (char*)&ec, &opt_len) == -1) {
					int _errno = wawo::net::core::SocketGetLastErrno() ;
					WAWO_LOG_FATAL( "SocketObserver_Abstract", "[#%d:%s] getsockopt failed: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), _errno );
					m_observer->PostIOEvent( WAWO_REF_PTR<MySocketT>(socket), IE_ERROR, WAWO_TRANSLATE_SOCKET_ERROR_CODE(_errno) );
				} else {
					//sometimes ,we get ec==0.
					if( ec == 0 ) {
						ec = wawo::E_SOCKET_UNKNOW_ERROR;
					}
					WAWO_LOG_DEBUG( "SocketObserver_Abstract", "[#%d:%s] socket io check error, code: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), ec );
					m_observer->PostIOEvent( WAWO_REF_PTR<MySocketT>(socket), IE_ERROR, WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec) );
				}

				flag &= ~SocketObserverEvent::EVT_ERROR ;
			}

			WAWO_ASSERT( flag == 0 );
		}

	public:
		SocketObserver_Abstract( MyObserverT * const observer ):
			m_observer(observer)
		{
		}
		virtual ~SocketObserver_Abstract() {
		}

		virtual void Init() {} ;
		virtual void Deinit() {
			m_socket_custom_evt.clear();
		};

		static void RegisterIOEvent( SocketEventPairPool& socket_evt_pairs, uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {

			WAWO_ASSERT( socket.Get() != NULL );
			WAWO_ASSERT( flag > 0 && flag <= 0xFFFF );

			typename SocketEventPairPool::iterator it = socket_evt_pairs.begin();
			while( it != socket_evt_pairs.end() ) {
			//operation system use fd to identify IO service
				if( it->socket->GetFd() == socket->GetFd() ) {
					it->flag |= flag ;
					WAWO_LOG_DEBUG( "SocketObserver_Abstract", "[#%d:%s] RegisterIOEvent: add: %d, now: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), flag, it->flag );
					return ;
				}
				++it;
			}

			MySocketAndEventConfigPair pair = {socket,flag};
			socket_evt_pairs.push_back( pair ) ;
			WAWO_LOG_DEBUG( "SocketObserver_Abstract", "[#%d:%s] RegisterIOEvent: add: %d, now: %d", socket->GetFd(), socket->GetAddr().AddressInfo(), flag, flag );
			return ;
		}

		inline static void UnRegisterIOEvent( SocketEventPairPool& socket_evt_pairs, uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {

			WAWO_ASSERT( socket.Get() != NULL );

			typename SocketEventPairPool::iterator check = socket_evt_pairs.begin();
			while( check != socket_evt_pairs.end() ) {

				if( check->socket->GetFd() == socket->GetFd() ) {

					check->flag &= ~flag;
					WAWO_LOG_DEBUG( "SocketObserver_Abstract", "[#%d:%s] UnRegisterIOEvent: remove: %d, left: %d",socket->GetFd(), socket->GetAddr().AddressInfo(), flag, check->flag );

					if( 0 == check->flag ) {
						WAWO_LOG_DEBUG( "SocketObserver_Abstract", "[#%d:%s] remove one socket", socket->GetFd(), socket->GetAddr().GetIpAddress() );
						socket_evt_pairs.erase( check ) ;
					}
					break;
				}
				++check;
			}
		}

	protected:
		virtual void CheckSystemIO() = 0;

	public:
		virtual void Register( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) = 0;
		virtual void UnRegister( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) = 0;
	};

}}}}

#endif //
