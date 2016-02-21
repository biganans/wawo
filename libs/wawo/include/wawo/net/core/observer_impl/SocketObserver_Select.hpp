#ifndef _WAWO_NET_CORE__OBSERVER_IMPL_SOCKET_OBSERVER_SELECT_HPP_
#define _WAWO_NET_CORE__OBSERVER_IMPL_SOCKET_OBSERVER_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.h>
#include <wawo/net/core/observer_impl/SocketObserver_Abstract.hpp>

#define WAWO_SELECT_MAX_PAIR 1000
#define WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME 64

namespace wawo { namespace net { namespace core { namespace observer_impl { 

	using namespace wawo::net ;
	using namespace wawo::thread;

	template <class _SocketT>
	class SocketObserver_Impl: 
		public SocketObserver_Abstract<_SocketT>
	{

		typedef _SocketT MySocketT;
		typedef SocketObserver<MySocketT> MySocketObserverT;

		struct SocketsToCheck {

			SocketsToCheck():
				max_fd_v(0)
			{
				FD_ZERO( &fds_r );
				FD_ZERO( &fds_w );
				FD_ZERO( &fds_ex );
				max_fd_v = 0;
				count = 0;
			}

			void Reset() {
				FD_ZERO( &fds_r );
				FD_ZERO( &fds_w );
				FD_ZERO( &fds_ex );
				max_fd_v = 0;
				count = 0;
			}

			MySocketT* sockets[WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME];
			int count;
			fd_set fds_r;
			fd_set fds_w;
			fd_set fds_ex;
			int max_fd_v;
		};

		public:
			SocketObserver_Impl( MySocketObserverT* const observer):
				SocketObserver_Abstract<_SocketT>(observer)
			{
				for( int i=0;i<WAWO_SELECT_MAX_PAIR;i++ ) {
					m_sockets_to_check[i] = new SocketsToCheck();
					WAWO_NULL_POINT_CHECK( m_sockets_to_check[i] );
				}
			}
			~SocketObserver_Impl() {
				for( int i=0;i<WAWO_SELECT_MAX_PAIR;i++ ) {
					WAWO_DELETE( m_sockets_to_check[i]) ;
				}
			}
	
			void Register( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {
				WAWO_ASSERT( flag > 0 );

				uint16_t _flag = 0;
				if( flag & SocketObserverEvent::EVT_READ ) {
					_flag |= SocketObserverEvent::EVT_READ;
				}
				if( flag & SocketObserverEvent::EVT_WRITE ) {
					_flag |= SocketObserverEvent::EVT_WRITE;
				}

				if( _flag != 0 ) {
					WAWO_CONDITION_CHECK( m_socket_evt_pairs.size() < WAWO_SELECT_LIMIT ) ;
					RegisterIOEvent( m_socket_evt_pairs, _flag, socket );
				}

				flag &= ~_flag; //clear sys io
				if( flag != 0 ) {
					_RegisterCustomIO( flag, socket );
				}

			}
			void UnRegister( uint16_t flag, WAWO_REF_PTR<MySocketT> const& socket ) {
				uint16_t _flag = 0;
				if( flag & SocketObserverEvent::EVT_READ ) {
					_flag |= SocketObserverEvent::EVT_READ;
				}
				if( flag & SocketObserverEvent::EVT_WRITE ) {
					_flag |= SocketObserverEvent::EVT_WRITE;
				}

				if( _flag != 0 ) {
					UnRegisterIOEvent( m_socket_evt_pairs, _flag, socket );
				}

				flag &= ~_flag ; //clear sys io
				if( flag != 0 ) {
					_UnRegisterCustomIO(flag, socket);
				}
			}

	public:
		void CheckSystemIO() {
			int fd_added_count = 0;
			int idx = 0;

			SocketEventPairPool::iterator it = m_socket_evt_pairs.begin();
			while( it != m_socket_evt_pairs.end() ) {
				WAWO_REF_PTR<MySocketT> const& socket = it->socket;

				if( socket->IsClosed() ) {
					//remove socket that has been closed
					it = m_socket_evt_pairs.erase( it );
					continue ;
				} else {

					if( (it->flag&SocketObserverEvent::EVT_READ) && (socket->GetRState() == S_READ_IDLE) ) {

						//if( socket->IsReadClosed() ) {
						//	m_observer->UnRegister( SocketObserverEvent::EVT_READ, socket );
						//	it++;
						//	continue;
						//}

						if( fd_added_count == WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_MAX_PAIR );
							fd_added_count = 0;
							m_sockets_to_check[idx]->max_fd_v = 0;
						}
					
						FD_SET( ( socket->GetFd() ), &m_sockets_to_check[idx]->fds_r );
						m_sockets_to_check[idx]->sockets[m_sockets_to_check[idx]->count++] = socket.Get() ;

						if( socket->GetFd() > m_sockets_to_check[idx]->max_fd_v ) {
							m_sockets_to_check[idx]->max_fd_v = socket->GetFd();
						}
						++fd_added_count;
					}

					if( (it->flag&SocketObserverEvent::EVT_WRITE) && 
							(socket->GetWState() == S_WRITE_IDLE)
						) {

						//if( socket->IsWriteClosed() ) {
						//	m_observer->UnRegister( SocketObserverEvent::EVT_WRITE, socket );
						//	it++;
						//	continue;
						//}

						if( fd_added_count == WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_MAX_PAIR );
							fd_added_count = 0;
							m_sockets_to_check[idx]->max_fd_v = 0;
						}

						FD_SET( ( socket->GetFd() ), &m_sockets_to_check[idx]->fds_w );
						m_sockets_to_check[idx]->sockets[m_sockets_to_check[idx]->count++] = socket.Get() ;

						if( socket->GetFd() > m_sockets_to_check[idx]->max_fd_v ) {
							m_sockets_to_check[idx]->max_fd_v = socket->GetFd();
						}

						++fd_added_count ;
					}

					if( (it->flag&SocketObserverEvent::EVT_WRITE) && 
						(socket->GetWState() == S_WRITE_IDLE) &&
						socket->IsConnecting() 
					) {
						//for async connect
						if( fd_added_count == WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_MAX_PAIR );
							fd_added_count = 0;
							m_sockets_to_check[idx]->max_fd_v = 0;
						}
					
						FD_SET( ( socket->GetFd() ), &m_sockets_to_check[idx]->fds_ex );
						m_sockets_to_check[idx]->sockets[m_sockets_to_check[idx]->count++] = socket.Get() ;
			
						if( socket->GetFd() > m_sockets_to_check[idx]->max_fd_v ) {
							m_sockets_to_check[idx]->max_fd_v = socket->GetFd();
						}
						++fd_added_count ;
					}
				}
				++it;
			}

			for( int j=0;j<=idx;j++ ) {
				if( m_sockets_to_check[j]->count > 0 ) {
					_CheckSocketsIO( m_sockets_to_check[j] );
					m_sockets_to_check[j]->Reset();
				}
			}
		}

	public:
		void Init() {
			SocketObserver_Abstract::Init();
			m_socket_evt_pairs.reserve( WAWO_SELECT_LIMIT );
		}
		void Deinit() {
			SocketObserver_Abstract::Deinit();
			m_socket_evt_pairs.clear();

			for( int i=0;i<WAWO_SELECT_MAX_PAIR;i++ ) {
				m_sockets_to_check[i]->Reset() ;
			}
		}

		void _CheckSocketsIO( SocketsToCheck* const sockets ) {
			WAWO_ASSERT( sockets->count <= WAWO_SELECT_CHECK_SOCKET_NUM_PER_TIME && sockets->count >0 );

			const timeval tv = {0,0};
			fd_set& fds_r = sockets->fds_r;
			fd_set& fds_w = sockets->fds_w;
			fd_set& fds_ex = sockets->fds_ex;

			int ready_c = select( sockets->max_fd_v , &fds_r, &fds_w, &fds_ex , &tv ); //only read now

			if( ready_c == 0 ) {
	//			WAWO_LOG_DEBUG("ConnectionObserver_Select","select timeout, no fd ready" );
				return ;
			} else if( ready_c == -1 ) {
				//notice 10038
				WAWO_LOG_FATAL("SocketObserver_Select","select error, errno: %d", wawo::net::core::SocketGetLastErrno() );
				return ;
			}

			//check begin
			for( int c_idx =0; (c_idx<sockets->count)&&(ready_c>0); c_idx++ ) {

				WAWO_REF_PTR<MySocketT> socket( sockets->sockets[c_idx] );

				//read and write check independently, so sometime times we get to this situation

				if( socket->IsClosed() ) {
	#ifdef _DEBUG
					//WAWO_ASSERT( socket->GetFd() == -1 );
	#endif
					continue;
				}

				int const& fd = socket->GetFd();
			
				//WAWO_ASSERT( fd_to_check > 0 );
				//WAWO_ASSERT( !socket->IsClosed() );

				if( FD_ISSET( fd, &fds_r ) ) {
					FD_CLR(fd, &fds_r);
					--ready_c ;
					_ProcessEvent( socket, SocketObserverEvent::EVT_READ );
				}

				if( FD_ISSET( fd, &fds_w ) ) {
					FD_CLR(fd, &fds_w);
					--ready_c;

					_ProcessEvent( socket, SocketObserverEvent::EVT_WRITE );
				}

				if( FD_ISSET(fd, &fds_ex) ) {
					FD_CLR(fd, &fds_ex);
					--ready_c ;

					WAWO_ASSERT( socket->IsDataSocket() );
					WAWO_ASSERT( socket->GetState() == Socket::STATE_CONNECTING ) ;

					_ProcessEvent( socket, SocketObserverEvent::EVT_ERROR );
					m_observer->AddUnRegisterOp( SocketObserverEvent::EVT_WRITE, socket );//only trigger once
				}
			}

		}

		SocketEventPairPool m_socket_evt_pairs;
		SocketsToCheck* m_sockets_to_check[WAWO_SELECT_MAX_PAIR] ;
	};

}}}}

#endif //