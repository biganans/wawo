#ifndef _WAWO_NET_CORE_OBSERVER_IMPL_EPOLL_HPP_
#define _WAWO_NET_CORE_OBSERVER_IMPL_EPOLL_HPP_

#include <sys/epoll.h>

#include <wawo/core.h>
#include <wawo/net/core/Observer_Abstract.hpp>

namespace wawo { namespace net { namespace core { namespace observer_impl {

	using namespace wawo::thread;

	class SocketObserver_Impl:
		public Observer_Abstract
	{
		typedef std::vector<SEData> SEDatas ;

	private:
		int m_epHandle;
		SEDatas m_sedatas;

		u64_t m_lst_check;
	public:
		SocketObserver_Impl( SocketObserver* const observer ):
			Observer_Abstract(observer),
			m_epHandle(-1),
			m_sedatas(),
			m_lst_check(0)
		{
		}

		~SocketObserver_Impl() {
			WAWO_ASSERT( m_epHandle == -1 );
		}

		void Watch( WWRP<Socket> const& socket, u16_t const& flag ) {

			WAWO_ASSERT( socket != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );
			WAWO_ASSERT( m_epHandle > 0 );

			if( socket->IsReadWriteShutdowned() ) { return ; }

			u16_t fflag = flag;

			if( (fflag&IOE_READ) | (fflag&IOE_WRITE) ) {

				u16_t flag_RDWR = 0;
				u16_t epoll_op ;

				SEDatas::iterator it = std::find_if( m_sedatas.begin(), m_sedatas.end(), [&socket]( SEData const& se_data ) {
					return socket == se_data.socket;
				});

				if( it == m_sedatas.end() ) {
					epoll_op = EPOLL_CTL_ADD;
					if( fflag&IOE_READ ) {
						flag_RDWR |= IOE_READ;
					}
					if( fflag&IOE_WRITE ) {
						flag_RDWR |= IOE_WRITE;
					}
				} else {
					epoll_op = EPOLL_CTL_MOD;
					if( (fflag&IOE_READ) || (it->flag&IOE_READ) ) {
						flag_RDWR |= IOE_READ;
					}
					if( (fflag&IOE_WRITE) || (it->flag&IOE_WRITE) ) {
						flag_RDWR |= IOE_WRITE;
					}
				}

				if( flag_RDWR != 0 ) {
					struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default
					epEvent.data.ptr = (void*) socket.Get();
					epEvent.events = EPOLLPRI;
		#ifdef WAWO_IO_MODE_EPOLL_USE_ET
					epEvent.events |= EPOLLET;
		#else
					epEvent.events |= EPOLLLT;
		#endif
					if( flag_RDWR&IOE_READ ) {
						epEvent.events |= EPOLLIN;
					}
					if( flag_RDWR&IOE_WRITE ) {
						epEvent.events |= EPOLLOUT;
					}

					WAWO_ASSERT( epoll_op == EPOLL_CTL_ADD || epoll_op == EPOLL_CTL_MOD );
					int nRet = epoll_ctl(m_epHandle,epoll_op,socket->GetFd(),&epEvent) ;

					if( nRet == -1 ) {
						WAWO_FATAL("[SocketObserver_Epoll][##%d][#%d:%s] epoll op failed, op code: %d, op flag: %d, error_code: %d", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR, SocketGetLastErrno() );
					} else {
						if( epoll_op == EPOLL_CTL_ADD ) {
							WAWO_DEBUG("[SocketObserver_Epoll][##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, EPOLL_CTL_ADD", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR );
							SEData se_data = {socket,flag_RDWR,0};
							m_sedatas.push_back(se_data);
						} else {
							WAWO_ASSERT( epoll_op == EPOLL_CTL_MOD );
							WAWO_DEBUG("[SocketObserver_Epoll][##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, old flag: %d", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR, it->flag );
							it->flag |= flag_RDWR;
						}
					}
				}
				fflag &= ~(IOE_READ|IOE_WRITE);
			}

			if( fflag != 0 ) {
				Observer_Abstract::_WatchCustomIO( socket, fflag ) ;
			}
		}
		void UnWatch( WWRP<Socket> const& socket, u16_t const& flag ) {

			WAWO_ASSERT( socket != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );
			WAWO_ASSERT( m_epHandle > 0 );

			u16_t fflag = flag;

			if( (fflag & IOE_READ) | (fflag & IOE_WRITE) ) {
				SEDatas::iterator it = std::find_if( m_sedatas.begin(), m_sedatas.end(), [&socket]( SEData const& se_data ) {
					return socket == se_data.socket;
				});

				if( it == m_sedatas.end() ) { return ; }

				if( socket->IsReadWriteShutdowned() ) {
					WAWO_DEBUG("[SocketObserver_Epoll][##%d][#%d:%s] socket closed already, remove from epoll, EPOLL_CTL_DEL", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					m_sedatas.erase(it);
					return ;
				}

				u16_t flag_RDWR = 0;
				u16_t epoll_op ;

				if( (fflag&IOE_READ) && (it->flag&IOE_READ) ) {
					flag_RDWR |= IOE_READ;
				}
				if( (fflag&IOE_WRITE) && (it->flag&IOE_WRITE) ) {
					flag_RDWR |= IOE_WRITE;
				}

				if( flag_RDWR != 0 ) {
					epoll_op = ((it->flag&(~flag_RDWR)) == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
					struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default

					if( epoll_op == EPOLL_CTL_MOD ) {
						epEvent.data.ptr = (void*) socket.Get();
						epEvent.events = EPOLLPRI ;
			#ifdef WAWO_IO_MODE_EPOLL_USE_ET
						epEvent.events |= EPOLLET;
			#else
						epEvent.events |= EPOLLLT;
			#endif
						if( it->flag&IOE_READ ) {
							epEvent.events |= EPOLLIN;
						}
						if( it->flag&IOE_WRITE ) {
							epEvent.events |= EPOLLOUT;
						}

						if( flag_RDWR&IOE_READ ) {
							epEvent.events &= ~EPOLLIN;
						}
						if( flag_RDWR&IOE_WRITE ) {
							epEvent.events &= ~EPOLLOUT;
						}
						WAWO_ASSERT( (epEvent.events&EPOLLIN) || (epEvent.events&EPOLLOUT) );
					}

					int nRet = epoll_ctl(m_epHandle,epoll_op,socket->GetFd(),&epEvent) ;

					if( nRet == -1 ) {
						WAWO_FATAL("[SocketObserver_Epoll][##%d][#%d:%s] epoll op failed, op code: %d, op flag: %d, error_code: %d", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR, SocketGetLastErrno() );
					} else {
						if( epoll_op == EPOLL_CTL_DEL ) {
							WAWO_DEBUG("[SocketObserver_Epoll][##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, EPOLL_CTL_DEL", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR );
							m_sedatas.erase(it);
						} else {
							WAWO_DEBUG("[SocketObserver_Epoll][##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, old flag: %d", m_epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(), epoll_op, flag_RDWR, it->flag );
							it->flag &= ~flag_RDWR;
						}
					}
				}
				fflag &= ~(IOE_READ|IOE_WRITE);
			}
			if( fflag != 0 ) {
				Observer_Abstract::_UnWatchCustomIO( socket, fflag );
			}
		}

	public:
		inline void CheckSystemIO() {
			_CheckEpollHandle( m_epHandle );
			_CheckSockets();
		}

		void Init() {
			Observer_Abstract::Init();

			m_epHandle = epoll_create( WAWO_EPOLL_SIZE );
			if( -1 == m_epHandle ) {
				WAWO_FATAL("[SocketObserver_Epoll]epoll create failed!, errno: %d", SocketGetLastErrno() );
				WAWO_THROW_EXCEPTION("create epoll handle failed");
			}

			WAWO_DEBUG("[SocketObserver_Epoll]init write epoll handle ok" );
		}
		void Deinit() {
			WAWO_DEBUG( "[SocketObserver_Epoll]Deinit ..." ) ;
			Observer_Abstract::Deinit();

			WAWO_CONDITION_CHECK( m_epHandle != -1);

			int rt = close( m_epHandle );
			if( -1 == rt ) {
				WAWO_FATAL("[SocketObserver_Epoll]epoll handle: %d, epoll close failed!: %d", m_epHandle, SocketGetLastErrno() );
				WAWO_THROW_EXCEPTION("SocketObserver_Epoll::deinit epoll handle failed");
			} else {
				WAWO_DEBUG("[SocketObserver_Epoll]deinit epoll handle ok" );
			}
			m_epHandle = -1;
			WAWO_DEBUG("[SocketObserver_Epoll] IOServiceObserver_Epoll::Deinit() done" ) ;
		}

	private:
		inline void _CheckEpollHandle( int const& epHandle ) {
			WAWO_ASSERT( epHandle > 0 );

			struct epoll_event epEvents[WAWO_EPOLL_PER_HANDLE_SIZE] ;
			int nTotalEvents = epoll_wait(epHandle, epEvents,WAWO_EPOLL_PER_HANDLE_SIZE,0);

			if ( -1 == nTotalEvents ) {
				WAWO_FATAL("[SocketObserver_Epoll]epoll wait event failed!, errno: %d", epHandle, SocketGetLastErrno() );
				return ;
			}

			for( int i=0;i<nTotalEvents;i++) {

				WAWO_ASSERT( epEvents[i].data.ptr != NULL );
				WWRP<Socket> socket (static_cast<Socket*> (epEvents[i].data.ptr)) ;

				wawo::u32_t events = ((epEvents[i].events) & 0xFFFFFFFF) ;

				WAWO_DEBUG( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: events(%d)",epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr(),events );

				if( events&EPOLLPRI ) {
					WAWO_FATAL( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: EPOLLPRI",epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					WAWO_THROW_EXCEPTION("EPOLLPRI arrive!!!");
					events &= ~EPOLLPRI;
				}

				if(events&EPOLLHUP) {
					UnWatch( socket, IOE_ALL );
					WAWO_DEBUG( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: (EPOLLHUP), close socket", epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					//Observer_Abstract::_ProcessEvent( socket, IOE_RST );
					//events &= ~EPOLLHUP; //NOT CONNECTED, OR CLOSED, BY RST

					socket->Close(wawo::E_SOCKET_RECEIVED_RST);
					continue;
				}

				if( events&EPOLLERR ) {
					WAWO_DEBUG( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: (EPOLLERR), post IOE_ERROR",epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					Observer_Abstract::_ProcessEvent( socket, IOE_ERROR );
					events &= ~(EPOLLERR);

					//110, -> EPOLLOUT,,why?
					if( socket->IsConnecting() ) {
						events &= ~EPOLLOUT;
					}
				}

				if( events&EPOLLIN ) {
					WAWO_DEBUG( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: EPOLLIN",epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					Observer_Abstract::_ProcessEvent(socket, IOE_READ);
					events &= ~EPOLLIN ;
				}

				if( events & EPOLLOUT ) {
					WAWO_DEBUG( "[SocketObserver_Epoll][##%d][#%d:%s] EVT: EPOLLOUT",epHandle, socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					Observer_Abstract::_ProcessEvent(socket, IOE_WRITE);
					events &= ~EPOLLOUT ;
				}

				WAWO_ASSERT( events == 0 );
			}
		}
		//check closed sockets, and remove from pairs
		inline void _CheckSockets() {

			u64_t curr = wawo::time::curr_seconds();
			if( m_lst_check == 0) {
				m_lst_check = curr;
				return ;
			}

			if( (curr-m_lst_check) > 2 ) {
				m_lst_check = curr;
				if( m_sedatas.size() > 0 ) {
					typename SEDatas::iterator it = m_sedatas.begin();
					while( it != m_sedatas.end() ) {
						if( it->socket->IsReadWriteShutdowned() ) {
							it = m_sedatas.erase(it);
						} else {
							++it;
						}
					}
				}
			}
		}
	};
}}}}
#endif
