#ifndef _WAWO_NET_CORE_OBSERVER_IMPL_SELECT_HPP_
#define _WAWO_NET_CORE_OBSERVER_IMPL_SELECT_HPP_

#include <vector>
#include <queue>

#include <wawo/core.h>
#include <wawo/net/core/Observer_Abstract.hpp>

#define WAWO_SELECT_BUCKET_MAX			1024
#define WAWO_SELECT_BUCKET_ITEM_COUNT	64
#define WAWO_SELECT_LIMIT (WAWO_SELECT_BUCKET_MAX*WAWO_SELECT_BUCKET_ITEM_COUNT)

namespace wawo { namespace net { namespace core { namespace observer_impl {

	using namespace wawo::net ;
	using namespace wawo::thread;


	class SocketObserver_Impl: 
		public Observer_Abstract
	{

	private:
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

			Socket* sockets[WAWO_SELECT_BUCKET_MAX];
			int count;
			fd_set fds_r;
			fd_set fds_w;
			fd_set fds_ex;
			int max_fd_v;
		};

	private:
		SEDatas m_sel_sedatas;
		SocketsToCheck* m_sockets_to_check[WAWO_SELECT_BUCKET_MAX] ;

		public:
			SocketObserver_Impl( SocketObserver* const observer):
				Observer_Abstract(observer)
			{
			}

			~SocketObserver_Impl() {
			}
	
			void Watch( WWRP<Socket> const& socket, u16_t const& flag ) {
				WAWO_ASSERT( flag > 0 );

				u16_t fflag = flag;
				u16_t flag_RDWR = 0;
				if( fflag & IOE_READ ) {
					flag_RDWR |= IOE_READ;
				}
				if( fflag & IOE_WRITE ) {
					flag_RDWR |= IOE_WRITE;
				}

				if( flag_RDWR != 0 ) {
					WAWO_CONDITION_CHECK( m_sel_sedatas.size() < WAWO_SELECT_LIMIT ) ;
					WatchIOEvent( m_sel_sedatas, socket, flag_RDWR );
				}

				fflag &= ~flag_RDWR; //clear sys io
				if( fflag != 0 ) {
					_WatchCustomIO( socket,fflag );
				}
			}

			void UnWatch( WWRP<Socket> const& socket, u16_t const& flag ) {
				WAWO_ASSERT( flag > 0 );

				u16_t fflag = flag;
				u16_t flag_RDWR = 0;

				if( fflag & IOE_READ ) {
					flag_RDWR |= IOE_READ;
				}
				if( fflag & IOE_WRITE ) {
					flag_RDWR |= IOE_WRITE;
				}

				if( flag_RDWR != 0 ) {
					UnWatchIOEvent( m_sel_sedatas, socket, flag_RDWR );
				}

				fflag &= ~flag_RDWR ; //clear sys io
				if( fflag != 0 ) {
					_UnWatchCustomIO(socket,fflag);
				}
			}

	public:
		void CheckSystemIO() {
			int fd_added_count = 0;
			int idx = 0;

			SEDatas::iterator it = m_sel_sedatas.begin();
			while( it != m_sel_sedatas.end() ) {
				WWRP<Socket> const& socket = it->socket;

				if( socket->IsReadWriteShutdowned() ) {
					WAWO_DEBUG( "[SocketObserver_Select][#%d:%s] remove one socket from m_sel_sedatas for ReadWriteShutdowned", socket->GetFd(), socket->GetRemoteAddr().AddressInfo().CStr() );
					it = m_sel_sedatas.erase( it );
					continue ;
				} else {

					if( (it->flag&IOE_READ) && (socket->GetRState() == S_READ_IDLE) && !socket->IsReadShutdowned() ) {

						if( fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_BUCKET_MAX );
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

					if( (it->flag&IOE_WRITE) && (socket->GetWState() == S_WRITE_IDLE) && !socket->IsWriteShutdowned()) {

						if( fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_BUCKET_MAX );
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

					if( (it->flag&IOE_WRITE) && 
						(socket->GetWState() == S_WRITE_IDLE) &&
						socket->IsConnecting() &&
						!socket->IsClosed()
					) {
						//for async connect
						if( fd_added_count == WAWO_SELECT_BUCKET_ITEM_COUNT ) {
							++idx;
							WAWO_ASSERT( idx < WAWO_SELECT_BUCKET_MAX );
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
				++it;
				}
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
			Observer_Abstract::Init();
			m_sel_sedatas.reserve( WAWO_SELECT_LIMIT );

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;i++ ) {
				m_sockets_to_check[i] = new SocketsToCheck();
				WAWO_NULL_POINT_CHECK( m_sockets_to_check[i] );
			}
		}
		void Deinit() {
			Observer_Abstract::Deinit();
			m_sel_sedatas.clear();

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;i++ ) {
				m_sockets_to_check[i]->Reset() ;
			}

			for( int i=0;i<WAWO_SELECT_BUCKET_MAX;i++ ) {
				WAWO_DELETE( m_sockets_to_check[i]) ;
			}
		}

		void _CheckSocketsIO( SocketsToCheck* const sockets ) {
			WAWO_ASSERT( sockets->count <= WAWO_SELECT_BUCKET_ITEM_COUNT && sockets->count >0 );

			const timeval tv = {0,0};
			fd_set& fds_r = sockets->fds_r;
			fd_set& fds_w = sockets->fds_w;
			fd_set& fds_ex = sockets->fds_ex;

			int ready_c = select( sockets->max_fd_v , &fds_r, &fds_w, &fds_ex , &tv ); //only read now

			if( ready_c == 0 ) {
				return ;
			} else if( ready_c == -1 ) {
				//notice 10038
				WAWO_FATAL("[SocketObserver_Select]select error, errno: %d", SocketGetLastErrno() );
				return ;
			}

			//check begin
			for( int c_idx =0; (c_idx<sockets->count)&&(ready_c>0); c_idx++ ) {

				WWRP<Socket> socket( sockets->sockets[c_idx] );
				//read and write check independently, so sometime times we get to this situation
				if( socket->IsReadWriteShutdowned() ) {
					continue;
				}

				int const& fd = socket->GetFd();
			
				if( FD_ISSET( fd, &fds_r ) ) {
					FD_CLR(fd, &fds_r);
					--ready_c ;
					_ProcessEvent( socket, IOE_READ );
				}

				if( FD_ISSET( fd, &fds_w ) ) {
					FD_CLR(fd, &fds_w);
					--ready_c;
					_ProcessEvent( socket, IOE_WRITE );
				}

				if( FD_ISSET(fd, &fds_ex) ) {
					FD_CLR(fd, &fds_ex);
					--ready_c ;

					WAWO_ASSERT( socket->IsDataSocket() );
					WAWO_ASSERT( socket->GetState() == S_CONNECTING ) ;

					_ProcessEvent( socket, IOE_ERROR );
					m_observer->AddUnWatchOp( socket,IOE_WRITE );//only trigger once
				}
			}
		}
	};
}}}}
#endif //