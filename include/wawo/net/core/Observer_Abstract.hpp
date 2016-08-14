#ifndef _WAWO_NET_CORE_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_
#define _WAWO_NET_CORE_OBSERVER_IMPL_SOCKET_OBSERVER_ABSTRACT_HPP_

#include <wawo/SmartPtr.hpp>
#include <wawo/net/NetEvent.hpp>
#include <wawo/net/SocketObserver.hpp>

namespace wawo { namespace net { namespace core {
	using namespace wawo::net;

	struct SEData {
		WWRP<Socket> socket ;
		u16_t flag;
		u16_t info; //not used right now
	};

	class Observer_Abstract:
		virtual public wawo::RefObject_Abstract
	{
	public:
		typedef std::vector<Socket> SocketPool;
		typedef std::vector<SEData> SEDatas ;

	protected:
		SocketObserver* m_observer;
		SEDatas m_custom_sedatas;
	public:
		inline void CheckCustomIO() {
			if( m_custom_sedatas.size() > 0 ) {
				SEDatas::iterator it = m_custom_sedatas.begin();
				while( it != m_custom_sedatas.end() ) {
					WWRP<Socket> const& socket = it->socket;

					if( (it->flag&IOE_DELAY_READ_BY_OBSERVER) && (socket->GetRState() == S_READ_IDLE) ) {
						_ProcessEvent( socket, IOE_DELAY_READ_BY_OBSERVER);
					} else if( (it->flag&IOE_DELAY_WRITE_BY_OBSERVER) && (socket->GetWState() == S_WRITE_IDLE) ) {
						_ProcessEvent( socket, IOE_DELAY_WRITE_BY_OBSERVER);
					} else {
						//WAWO_ASSERT( !"unknown socket check flag");
					}

					++it;
				}
			}
		}

		inline void _WatchCustomIO( WWRP<Socket> const& socket, u16_t const& flag) {
			WatchIOEvent( m_custom_sedatas, socket, flag );
		}

		inline void _UnWatchCustomIO( WWRP<Socket> const& socket, u16_t const& flag) {
			UnWatchIOEvent( m_custom_sedatas, socket, flag );
		}

		inline void _ProcessEvent( WWRP<Socket> const& socket, wawo::u16_t const& flag, wawo::u16_t const& info = 0 ) {

			(void)&info;
			u16_t fflag = flag;

			if( fflag&IOE_RD ) {

				if( socket->IsReadShutdowned() ) {
					m_observer->AddUnWatchOp(socket, IOE_RD);
				} else {
					#ifdef WAWO_IO_MODE_SELECT
					WAWO_ASSERT( socket->GetRState() == S_READ_IDLE );
					#endif

					UniqueLock<SpinMutex> lg( socket->GetLock(L_READ), wawo::thread::try_to_lock );
					bool lck_ok = lg.own_lock() ;

					//for remote send,,posted, remote send and arrive before local read case
					if( !lck_ok || socket->GetRState() != S_READ_IDLE ) {
						if( (fflag&IOE_READ) ) {
							m_observer->AddWatchOp(socket, IOE_DELAY_READ_BY_OBSERVER );
							m_observer->AddUnWatchOp( socket, IOE_READ );
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

						if( (fflag&IOE_DELAY_READ_BY_OBSERVER) ) {
							m_observer->AddWatchOp( socket, IOE_READ );
							m_observer->AddUnWatchOp( socket, IOE_DELAY_READ_BY_OBSERVER );
						}
					}
				}
				fflag &= ~IOE_RD;
			}

			if( fflag & IOE_WR ) {
				WAWO_ASSERT( socket->IsDataSocket() );

				if( socket->IsWriteShutdowned() ) {
					m_observer->AddUnWatchOp(socket, IOE_WR);
				} else {

					UniqueLock<SpinMutex> lg( socket->GetLock(L_WRITE), wawo::thread::try_to_lock );
					bool lck_ok = lg.own_lock() ;

					if( !lck_ok || (socket->GetWState() != S_WRITE_IDLE)) {
						//
						WAWO_WARN("[Observer_Abstract][#%d:%s] socket lock write failed, delay write state: %d", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), socket->GetWState() );

						if( (fflag&IOE_WRITE) ) {
							m_observer->AddWatchOp( socket, IOE_DELAY_WRITE_BY_OBSERVER );
							m_observer->AddUnWatchOp( socket, IOE_WRITE );
						}
					} else {

						WAWO_ASSERT( socket->GetWState() == S_WRITE_IDLE );

						if( socket->IsConnecting() ) {
							m_observer->AddUnWatchOp( socket, IOE_WRITE );
							socket->SetWState( S_WRITE_POSTED ) ;
							m_observer->PostIOEvent(socket, IE_CONNECTED);
						} else {

							if(socket->GetWBuffer()->BytesCount() == 0) {
								m_observer->AddUnWatchOp( socket, IOE_WR );
							}
							socket->SetWState( S_WRITE_POSTED ) ;
							m_observer->PostIOEvent(socket, IE_CAN_WRITE);
						}

						if( (fflag&IOE_DELAY_WRITE_BY_OBSERVER) ) {
							m_observer->AddWatchOp( socket, IOE_WRITE );
							m_observer->AddUnWatchOp( socket, IOE_DELAY_WRITE_BY_OBSERVER );
						}
					}
				}
				fflag &= ~IOE_WR;
			}

			if(fflag&IOE_ERROR) {
				UniqueLock<SpinMutex> lg( socket->GetLock(L_SOCKET) );

				int ec =0 ;
				socklen_t opt_len = sizeof(int);
				if (getsockopt( socket->GetFd(), SOL_SOCKET, SO_ERROR, (char*)&ec, &opt_len) == -1) {
					int _errno = SocketGetLastErrno() ;
					ec = _errno;
					WAWO_FATAL( "[Observer_Abstract][#%d:%s] getsockopt failed: %d", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), _errno );
				}
				WAWO_ASSERT( ec != 0);

				m_observer->PostIOEvent( WWRP<Socket>(socket), IE_ERROR, WAWO_TRANSLATE_SOCKET_ERROR_CODE(ec) );
				fflag &= ~IOE_ERROR ;
			}

			WAWO_ASSERT( fflag == 0 );
		}

	public:
		Observer_Abstract( SocketObserver* const observer ):
			m_observer(observer),
			m_custom_sedatas()
		{
		}
		virtual ~Observer_Abstract() {}

		virtual void Init() {
			m_custom_sedatas.reserve(4096);
		};
		virtual void Deinit() {
			m_custom_sedatas.clear();
		};

		inline static void WatchIOEvent( SEDatas& se_datas, WWRP<Socket> const& socket, u16_t const& flag ) {

			WAWO_ASSERT( socket != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );

			u16_t fflag = flag;
			SEDatas::iterator it = se_datas.begin();
			while( it != se_datas.end() ) {
				if( it->socket == socket ) {
					it->flag |= fflag ;
					WAWO_DEBUG( "[Observer_Abstract][#%d:%s] RegisterIOEvent: add new flag: %d, now: %d", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), fflag, it->flag );
					return ;
				} else { ++it; }
			}

			SEData pair = {socket,fflag,0};
			se_datas.push_back( pair ) ;
		}

		inline static void UnWatchIOEvent( SEDatas& se_datas, WWRP<Socket> const& socket, u16_t const& flag ) {

			WAWO_ASSERT( socket != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );
			WAWO_ASSERT( flag > 0 && flag <= 0xFFFF );

			u16_t fflag = flag;

			SEDatas::iterator it = se_datas.begin();
			while( it != se_datas.end() ) {
				if( it->socket == socket ) {
					it->flag &= ~fflag;
					WAWO_DEBUG( "[Observer_Abstract][#%d:%s] UnRegisterIOEvent: remove: %d, left: %d",socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), fflag, it->flag );

					if( 0 == it->flag ) {
						WAWO_DEBUG( "[Observer_Abstract][#%d:%s] remove one socket from pair", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
						se_datas.erase( it ) ;
					}
					break;
				} else { ++it; }
			}
		}

	protected:
		virtual void CheckSystemIO() = 0;

	public:
		virtual void Watch( WWRP<Socket> const& socket, u16_t const& flag ) = 0;
		virtual void UnWatch( WWRP<Socket> const& socket, u16_t const& flag ) = 0;
	};

}}}
#endif //