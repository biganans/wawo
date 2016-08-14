#ifndef _WAWO_NET_SOCKET_OBSERVER_HPP_
#define _WAWO_NET_SOCKET_OBSERVER_HPP_

#include <queue>
#include <vector>

#include <wawo/core.h>
#include <wawo/thread/Thread.hpp>
#include <wawo/SmartPtr.hpp>
#include <wawo/task/Task.hpp>

#include <wawo/net/Socket.hpp>

namespace wawo { namespace net { namespace core { namespace observer_impl {
	class SocketObserver_Impl;
}}}}

namespace wawo { namespace net {
	using namespace wawo::thread;

	class SocketObserver;

	class IOEventTask :
		public wawo::task::Task_Abstract,
		public wawo::net::core::Event
	{
		WWRP<SocketObserver> m_observer;
		WWRP<Socket> m_socket;
	public:
		explicit IOEventTask(WWRP<SocketObserver> const& observer, WWRP<Socket> const& socket, u32_t const& eid, i32_t const& ec) :
			Task_Abstract(),
			Event(eid, core::Cookie(ec)),
			m_observer(observer),
			m_socket(socket)
		{
		}
		bool Isset() {
			return (m_socket != NULL) && (m_observer != NULL);
		}

		void Reset() {
			m_socket = NULL;
			m_observer = NULL;
		}

		void Run();
		void Cancel();
	};

	class SocketObserver:
		public ThreadRunObject_Abstract
	{
		enum EventOpCode {
			OP_WATCH,
			OP_UNWATCH
		};

		struct EventOp {
			WWRP<Socket> socket;
			u16_t flag;
			EventOpCode op:16;
		};
		typedef std::queue<EventOp> EventOpQueue ;

		enum SocketObserverState {
			S_IDLE = 0,
			S_RUN,
			S_EXIT
		};

		void _ProcessOps() ;
	public:
		SocketObserver( bool auto_start=false ) ;
		~SocketObserver() ;

		void AllocImpl();
		void DeAllocImpl() ;

		void Stop();
		void OnStart() ;
		void OnStop() ;
		void Run();

		//add socket to async io check list
		inline void Watch( WWRP<Socket> const& socket, u16_t const& flag ) {
			//SharedLockGuard<SharedMutex> lg(m_mutex);
			//if( m_state != S_RUN ) {
			//	return ;
			//}

			//WAWO_ASSERT( m_impl != NULL );
			AddWatchOp( socket,flag );
		}
		//remote socket from async io check list for the specified evt_flag
		inline void UnWatch( WWRP<Socket> const& socket,u16_t const& flag ) {
			//SharedLockGuard<SharedMutex> lg(m_mutex);
			//if( m_state != S_RUN ) {
			//	return ;
			//}

			//WAWO_ASSERT( m_impl != NULL );
			AddUnWatchOp( socket,flag );
		}

		inline void AddWatchOp(WWRP<Socket> const& socket, u16_t const& flag) {
			WAWO_ASSERT(socket != NULL);
			WAWO_ASSERT(socket->IsNonBlocking());
			EventOp op = {
				socket,
				flag,
				OP_WATCH
			};

			LockGuard<SpinMutex> _lg(m_ops_mutex);
			m_ops.push(op);
		}
		inline void AddUnWatchOp(WWRP<Socket> const& socket, u16_t const& flag) {
			WAWO_ASSERT(socket != NULL);
			WAWO_ASSERT(socket->IsNonBlocking());
			EventOp op = {
				socket,
				flag,
				OP_UNWATCH
			};
			LockGuard<SpinMutex> _lg(m_ops_mutex);
			m_ops.push(op);
		}

		inline void PostIOEvent( WWRP<Socket> const& socket, u32_t const& eid, int const& ec = 0) {
			WWRP<wawo::task::Task_Abstract> ioet(new IOEventTask(WWRP<SocketObserver>(this), socket, eid, ec));
			WAWO_NET_CORE_IO_TASK_MANAGER->Schedule(ioet);
		}

	private:
		SharedMutex m_mutex;
		SocketObserverState m_state;
		core::observer_impl::SocketObserver_Impl* m_impl;

		SpinMutex m_ops_mutex;
		EventOpQueue m_ops;
	};

}} //ns of wawo/net
#endif //