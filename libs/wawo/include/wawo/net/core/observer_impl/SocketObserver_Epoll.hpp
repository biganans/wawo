#ifndef _WAWO_NET_CORE_OBSERVER_SOCKET_OBSERVER_EPOLL_HPP_
#define _WAWO_NET_CORE_OBSERVER_SOCKET_OBSERVER_EPOLL_HPP_

#include <sys/epoll.h>

#include <wawo/core.h>
#include <wawo/net/core/observer_impl/SocketObserver_Abstract.hpp>


namespace wawo { namespace net { namespace core { namespace observer_impl {

	using namespace wawo::thread;

	template <class _SocketT>
	class SocketObserver_Impl:
		public SocketObserver_Abstract<_SocketT>
	{

		typedef _SocketT MySocketT;
		typedef SocketObserver<MySocketT> MySocketObserverT;

		typedef SocketAndEventConfigPair<_SocketT> MySocketAndEventConfigPair;
		typedef std::vector<MySocketAndEventConfigPair> SocketEventPairPool ;
		typedef SocketObserver_Abstract<_SocketT> MySocketObserve_AbstractT;

	public:
		SocketObserver_Impl( MySocketObserverT* const observer ):
			MySocketObserve_AbstractT(observer),
			m_epHandle(-1),
			m_lst_check(0)
		{
		}

		~SocketObserver_Impl() {
			WAWO_ASSERT( m_epHandle == -1 );
		}

		void Register( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( socket.Get() != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );
			WAWO_ASSERT( m_epHandle > 0 );

			if( socket->IsClosed() ) {
				return ;
			}

			if( (flag & SocketObserverEvent::EVT_READ) | (flag & SocketObserverEvent::EVT_WRITE) ) {

				typename SocketEventPairPool::iterator it = std::find_if( m_socket_evt_pairs.begin(), m_socket_evt_pairs.end(), [&]( MySocketAndEventConfigPair const& pair ) {
					return socket == pair.socket;
				});

				uint16_t _flag = 0;
				uint16_t _epoll_op ;

				if( it == m_socket_evt_pairs.end() ) {
					_epoll_op = EPOLL_CTL_ADD;
					if( flag&SocketObserverEvent::EVT_READ ) {
						_flag |= SocketObserverEvent::EVT_READ;
					}
					if( flag&SocketObserverEvent::EVT_WRITE ) {
						_flag |= SocketObserverEvent::EVT_WRITE;
					}
				} else {
					_epoll_op = EPOLL_CTL_MOD;
					if( (flag&SocketObserverEvent::EVT_READ) || (it->flag&SocketObserverEvent::EVT_READ) ) {
						_flag |= SocketObserverEvent::EVT_READ;
					}
					if( (flag&SocketObserverEvent::EVT_WRITE) || (it->flag&SocketObserverEvent::EVT_WRITE) ) {
						_flag |= SocketObserverEvent::EVT_WRITE;
					}
				}

				if( _flag != 0 ) {
					struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default
					epEvent.data.ptr = (void*) socket.Get();
					epEvent.events = EPOLLPRI;
		#ifdef WAWO_IO_MODE_EPOLL_USE_ET
					epEvent.events |= (EPOLLET | EPOLLRDHUP);
		#endif
					if( _flag&SocketObserverEvent::EVT_READ ) {
						epEvent.events |= EPOLLIN;
					}
					if( _flag&SocketObserverEvent::EVT_WRITE ) {
						epEvent.events |= EPOLLOUT;
					}
					int nRet = epoll_ctl(m_epHandle,_epoll_op,socket->GetFd(),&epEvent) ;

					if( nRet == -1 ) {
						WAWO_LOG_FATAL("SocketObserver_Epoll","[##%d][#%d:%s] epoll op failed, op code: %d, op flag: %d, error_code: %d", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag, wawo::net::core::SocketGetLastErrno() );
					} else {
						if( _epoll_op == EPOLL_CTL_ADD ) {
							WAWO_LOG_DEBUG("SocketObserver_Epoll","[##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, EPOLL_CTL_ADD", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag );
							MySocketAndEventConfigPair pair = {socket,_flag};
							m_socket_evt_pairs.push_back(pair);
						} else {
							WAWO_LOG_DEBUG("SocketObserver_Epoll","[##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, old flag: %d", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag, it->flag );
							it->flag |= _flag;
						}
					}
				}

				flag &= ~(SocketObserverEvent::EVT_READ|SocketObserverEvent::EVT_WRITE);
			}

			if( flag != 0 ) {
				MySocketObserve_AbstractT::_RegisterCustomIO( flag, socket ) ;
			}
		}
		void UnRegister( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {
			WAWO_ASSERT( socket.Get() != NULL );
			WAWO_ASSERT( socket->IsNonBlocking() );

			if( (flag & SocketObserverEvent::EVT_READ) | (flag & SocketObserverEvent::EVT_WRITE) ) {
				typename SocketEventPairPool::iterator it = std::find_if( m_socket_evt_pairs.begin(), m_socket_evt_pairs.end(), [&]( MySocketAndEventConfigPair const& pair ) {
					return socket == pair.socket;
				});

				//WAWO_ASSERT( it != m_socket_evt_pairs.end() );

				if( it == m_socket_evt_pairs.end() ) {
					return ;
				}

				if( socket->IsClosed() ) {
					WAWO_LOG_DEBUG("SocketObserver_Epoll","[##%d][#%d:%s] socket closed already, remove from epoll, EPOLL_CTL_DEL", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );
					m_socket_evt_pairs.erase(it);
					return ;
				}

				WAWO_ASSERT( socket->GetFd() > 0 );

				uint16_t _flag = 0;
				uint16_t _epoll_op ;

				if( (flag&SocketObserverEvent::EVT_READ) && (it->flag&SocketObserverEvent::EVT_READ) ) {
					_flag |= SocketObserverEvent::EVT_READ;
				}
				if( (flag&SocketObserverEvent::EVT_WRITE) && (it->flag&SocketObserverEvent::EVT_WRITE) ) {
					_flag |= SocketObserverEvent::EVT_WRITE;
				}

				if( _flag !=0 ) {
					_epoll_op = ((it->flag&(~_flag)) == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
					struct epoll_event epEvent; //EPOLLHUP | EPOLLERR always added by default

					if( _epoll_op == EPOLL_CTL_MOD ) {
						epEvent.data.ptr = (void*) socket.Get();
						epEvent.events = EPOLLPRI ;
			#ifdef WAWO_IO_MODE_EPOLL_USE_ET
						epEvent.events |= (EPOLLET | EPOLLRDHUP);
			#endif
						if( it->flag&SocketObserverEvent::EVT_READ ) {
							epEvent.events |= EPOLLIN;
						}
						if( it->flag&SocketObserverEvent::EVT_WRITE ) {
							epEvent.events |= EPOLLOUT;
						}

						if( _flag&SocketObserverEvent::EVT_READ ) {
							epEvent.events &= ~EPOLLIN;
						}
						if( _flag&SocketObserverEvent::EVT_WRITE ) {
							epEvent.events &= ~EPOLLOUT;
						}
						WAWO_ASSERT( (epEvent.events&EPOLLIN) || (epEvent.events&EPOLLOUT) );
					}

					int nRet = epoll_ctl(m_epHandle,_epoll_op,socket->GetFd(),&epEvent) ;

					if( nRet == -1 ) {
						WAWO_LOG_FATAL("SocketObserver_Epoll","[##%d][#%d:%s] epoll op failed, op code: %d, op flag: %d, error_code: %d", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag, wawo::net::core::SocketGetLastErrno() );
					} else {

						if( _epoll_op == EPOLL_CTL_DEL ) {
							WAWO_LOG_DEBUG("SocketObserver_Epoll","[##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, EPOLL_CTL_DEL", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag );
							m_socket_evt_pairs.erase(it);
						} else {
							WAWO_LOG_DEBUG("SocketObserver_Epoll","[##%d][#%d:%s] epoll op success, op code: %d, op flag: %d, old flag: %d", m_epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(), _epoll_op, _flag, it->flag );
							it->flag &= ~_flag;
						}
					}
				}

				flag &= ~(SocketObserverEvent::EVT_READ|SocketObserverEvent::EVT_WRITE);
			}

			if( flag != 0 ) {
				MySocketObserve_AbstractT::_UnRegisterCustomIO(flag, socket );
			}
		}

	public:
		inline void CheckSystemIO() {
			_CheckEpollHandle( m_epHandle );
			_CheckSockets();
		}

		void Init() {
			MySocketObserve_AbstractT::Init();

			m_epHandle = epoll_create( WAWO_EPOLL_SIZE );
			if( -1 == m_epHandle ) {
				WAWO_LOG_FATAL("SocketObserver_Epoll","epoll create failed!, errno: %d", wawo::net::core::SocketGetLastErrno() );
				WAWO_THROW_EXCEPTION("create epoll handle failed");
			}

			WAWO_LOG_DEBUG("SocketObserver_Epoll","init write epoll handle ok" );
		}
		void Deinit() {
			WAWO_LOG_DEBUG( "epoll", "Deinit ..." ) ;
			MySocketObserve_AbstractT::Deinit();

			WAWO_CONDITION_CHECK( m_epHandle != -1);

			int rt = close( m_epHandle );
			if( -1 == rt ) {
				WAWO_LOG_FATAL("epoll","epoll handle: %d, epoll close failed!: %d", m_epHandle, wawo::net::core::SocketGetLastErrno() );
				WAWO_THROW_EXCEPTION("SocketObserver_Epoll::deinit epoll handle failed");
			} else {
				WAWO_LOG_DEBUG("epoll","deinit epoll handle ok" );
			}
			m_epHandle = -1;
			WAWO_LOG_DEBUG("epoll","IOServiceObserver_Epoll::Deinit() done" ) ;
		}

		inline void _CheckEpollHandle( int const& epHandle ) {
			WAWO_ASSERT( epHandle > 0 );

			struct epoll_event epEvents[2048] ;
			int nTotalEvents = epoll_wait(epHandle, epEvents,2048,0);

			if ( -1 == nTotalEvents ) {
				WAWO_LOG_FATAL("IOServiceObserver_Epoll","epoll wait event failed!, errno: %d", epHandle, wawo::net::core::SocketGetLastErrno() );
				return ;
			}

			for( int i=0;i<nTotalEvents;i++) {

				WAWO_REF_PTR<MySocketT> socket (static_cast<MySocketT*> (epEvents[i].data.ptr)) ;
				WAWO_ASSERT( socket != NULL );
				wawo::uint32_t events = ((epEvents[i].events) & 0xFFFFFFFF) ;

				WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: events(%d)",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo(),events );

				if( events&EPOLLPRI ) {
					WAWO_LOG_FATAL( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: EPOLLPRI",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );
					WAWO_THROW_EXCEPTION("EPOLLPRI arrive!!!");
					events &= ~EPOLLPRI;
				}

				//THIS IS A OPTIMIZED FOR EPOLL CTL, CAUSE, out would trigger unregister -> epoll_ctl, but we have done that on above lines...

	#ifdef WAWO_IO_MODE_EPOLL_USE_ET
				if( events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP)) {
	#else
				if( events&(EPOLLERR|EPOLLHUP)) {

	#endif
					UnRegister(SocketObserverEvent::EVT_READ|SocketObserverEvent::EVT_WRITE,socket);

					if(events&EPOLLHUP) {
						WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: (EPOLLHUP), ignore",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );

						#ifndef WAWO_IO_MODE_EPOLL_USE_ET
						WAWO_ASSERT( events&EPOLLIN ); // when work as LT mode we should have this..., or we'll lose close call
						#endif

						events &= ~EPOLLHUP; //NOT CONNECTED, OR CLOSED, BY RST
					}
					//for error code read first reason, if RDHUP first,,we may lose error code
					if( events&EPOLLERR ) {
						WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: (EPOLLERR), post EVT_ERROR",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );
						MySocketObserve_AbstractT::_ProcessEvent( socket, SocketObserverEvent::EVT_ERROR );
						events &= ~(EPOLLERR|EPOLLIN|EPOLLOUT); //ERROR //ignore in and out
					}

	#ifdef WAWO_IO_MODE_EPOLL_USE_ET
					if( events&EPOLLRDHUP) {
						WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: (EPOLLRDHUP), remote grace close, post EVT_CLOSE, ignore EPOLLIN if any,,",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );

						MySocketObserve_AbstractT::_ProcessEvent( socket, SocketObserverEvent::EVT_CLOSE );
						events &= ~(EPOLLRDHUP|EPOLLIN); // POST IN ,ignore IN
					}
	#endif
				}

				if( events & EPOLLIN ) {
					WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: EPOLLIN",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );
					MySocketObserve_AbstractT::_ProcessEvent(socket, SocketObserverEvent::EVT_READ);
					events &= ~EPOLLIN ;
				}

				if( events & EPOLLOUT ) {
					WAWO_LOG_DEBUG( "SocketObserver_Epoll", "[##%d][#%d:%s] EVT: EPOLLOUT",epHandle, socket->GetFd(), socket->GetAddr().AddressInfo() );
					MySocketObserve_AbstractT::_ProcessEvent(socket, SocketObserverEvent::EVT_WRITE);
					events &= ~EPOLLOUT ;
				}

				WAWO_ASSERT( events == 0 );
			}
		}
		//check closed sockets, and remove from pairs
		inline void _CheckSockets() {

			uint64_t curr = wawo::time::curr_seconds();
			if( m_lst_check == 0) {
				m_lst_check = curr;
				return ;
			}

			if( (curr-m_lst_check) > 2 ) {
				m_lst_check = curr;
				if( m_socket_evt_pairs.size() > 0 ) {
					typename SocketEventPairPool::iterator it = m_socket_evt_pairs.begin();
					while( it != m_socket_evt_pairs.end() ) {
						if( it->socket->IsClosed() ) {
							it = m_socket_evt_pairs.erase(it);
						} else {
							++it;
						}
					}
				}
			}
		}

		int m_epHandle;
		SocketEventPairPool m_socket_evt_pairs;

		uint64_t m_lst_check;
	};

}}}}
#endif //