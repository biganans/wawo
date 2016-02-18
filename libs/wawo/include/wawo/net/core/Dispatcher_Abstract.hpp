#ifndef _WAWO_NET_CORE_DISPATCHER_HPP_
#define _WAWO_NET_CORE_DISPATCHER_HPP_

#include <vector>
#include <queue>
#include <map>

#include <wawo/thread/Mutex.h>
#include <wawo/log/LoggerManager.h>

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/core/IOTaskManager.hpp>

#include <wawo/SmartPtr.hpp>


//#define USE_SHARED_MUTEX

namespace wawo { namespace net { namespace core {

	using namespace wawo::thread;


	/* RULES
	 * 1, you must register the relevant evt id before your listener can receive your evt
	 * 2, you must unregister the relevant evt ids before your listener be deleted
	 * 3, break 1, your listener receive nothing (obviously)
	 * 4, break 2, behaviour undefined
	 */

	//template <class E>
	//class Listener_Abstract;

	//E, event to dispatch, E must be subclass of Event
	//L, Listener

	template <class E>
	class PostTask;

	template <class E>
	class RaiseTask;

	template <class E>
	class Dispatcher_Abstract :
		virtual public wawo::RefObject_Abstract
	{

	public:
		typedef PostTask<E>		_PostTaskT;
		typedef RaiseTask<E>	_RaiseTaskT;

		typedef Listener_Abstract<E> _MyListenerT;
		typedef Dispatcher_Abstract<E> _MyDispatcherT;

		enum ListenerOpCode {
			OP_ADD_ID_LISTENER = 0,
			OP_REMOVE_ID_LISTENER,
			OP_REMOVE_ID_ALL_LISTENER,
			OP_REMOVE_LISTENER_ALL_ID
		};

		struct ListenerOp {
			ListenerOpCode code:8; //default on
			int id:24;
			WAWO_REF_PTR<_MyListenerT> listener;

			ListenerOp( ListenerOpCode code , int const& id, WAWO_REF_PTR<_MyListenerT> const& l ):
				code(code),
				id(id),
				listener(l)
			{
			}

			bool operator == ( ListenerOp const& second ) {
				return (code == second.code)
						&& (id == second.id)
						&& (listener == second.listener)
					;
			}
		};

		typedef std::queue<ListenerOp> _ListenerToOpQueue;
		typedef std::vector< WAWO_REF_PTR<_MyListenerT> > _ListenerVector;
		typedef typename _ListenerVector::iterator _MyListenerIterator;

		typedef std::map< int, _ListenerVector* > _IdListenerMap;
		typedef typename _IdListenerMap::iterator _MyIdListenerIterator;

		//TUNE BETWEEN SharedSpinMutex AND SharedMutex FOR PERFORMACE BENCHMARK ON DIFFERENT PLATFORM
#ifdef USE_SHARED_MUTEX
		typedef wawo::thread::SharedMutex _MyIdListenerMapMutex;
#else
		typedef wawo::thread::SpinMutex _MyIdListenerMapMutex;
#endif

	private:
		_MyIdListenerMapMutex m_id_listener_map_mutex;
		_IdListenerMap m_id_listener_map;

		SpinMutex m_listener_ops_mutex;
		_ListenerToOpQueue m_listener_ops ;
	public:
		Dispatcher_Abstract() {}

		virtual ~Dispatcher_Abstract() {
			_ProcessListenerOp();
			UnRegisterAll();

			WAWO_LOG_DEBUG( "Dispatcher", "~Dispatcher, address: 0x%p", this ) ;
		}

		void UnRegisterAll() {
			LockGuard<_MyIdListenerMapMutex> _lg( m_id_listener_map_mutex );
			_MyIdListenerIterator it = m_id_listener_map.begin();

			uint32_t count = 0;
			while ( it != m_id_listener_map.end() ) {
				_MyIdListenerIterator it_to_unregister = it;
				++it;
				count += it_to_unregister->second->size();
				it_to_unregister->second->clear();
				WAWO_DELETE( it_to_unregister->second );
				m_id_listener_map.erase( it_to_unregister );
			}

			WAWO_LOG_DEBUG( "dispatcher_abstract", "unregister all, count: %d", count ) ;
		}


		void Register( int const& id, WAWO_REF_PTR<_MyListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_ADD_ID_LISTENER, id, listener );
			if( force_update ) {
				LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		//don't use force_update !!!
		void UnRegister( int const& id, WAWO_REF_PTR<_MyListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_REMOVE_ID_LISTENER, id, listener );
			if( force_update ) {
				LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		void UnRegister( WAWO_REF_PTR<_MyListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_REMOVE_LISTENER_ALL_ID,-1,listener );
			if( force_update ) {
				LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		void UnRegister( int const& id, bool const& force_update = false) {
			_AddListenerOp( OP_REMOVE_ID_ALL_LISTENER,id, NULL );
			if( force_update ) {
				LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		//block until the task execute complete
		inline void Trigger( WAWO_REF_PTR<E> const& evt ) {
			Dispatch(evt,true);
		}
		//return immediately, plan a execute task on task manager
		//inline void Post( WAWO_REF_PTR<E> const& evt ) {
		//	Dispatch(evt,false) ;
		//}
		//return immediately , plan a dispatcher on task manager, and then call Trigger
		inline void Raise( WAWO_REF_PTR<E> const& evt ) {
			WAWO_REF_PTR<wawo::task::Task_Abstract> evt_task( new _RaiseTaskT( WAWO_REF_PTR< _MyDispatcherT >(this), evt ) );
			WAWO_IO_TASK_MANAGER->PlanTask( evt_task );
		}

	protected:
		inline void ProcessListenerOp() {
#ifdef USE_SHARED_MUTEX
			SharedLockGuard _lg(m_id_listener_map_mutex) ;
#else
			LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
#endif

			if( !m_listener_ops.empty() ) {
#ifdef USE_SAHRED_MUTEX
				SharedUpgradeToLockGuard _lg(m_id_listener_map_mutex) ;
#endif
				_ProcessListenerOp();
			}
		}
	private:
		inline void _AddListenerOp( ListenerOpCode const& code, int const& id, WAWO_REF_PTR<_MyListenerT> const& listener ) {
			LockGuard<SpinMutex> _ls( m_listener_ops_mutex );
			ListenerOp op (code,id,listener);
			m_listener_ops.push( op );
		}
		inline void _ProcessListenerOp() {
			LockGuard<SpinMutex> _ls( m_listener_ops_mutex );

			while( !m_listener_ops.empty() ) {

				ListenerOp listener_op = m_listener_ops.front();
				m_listener_ops.pop();

				ListenerOpCode const& op_code = listener_op.code ;
				int const& id = listener_op.id;
				WAWO_REF_PTR<_MyListenerT> const& listener = listener_op.listener;

				switch( op_code ) {

					case OP_ADD_ID_LISTENER:
						{
							WAWO_ASSERT( listener_op.id > 0 );
							WAWO_ASSERT( listener != NULL );

							_MyIdListenerIterator it_map = m_id_listener_map.find( listener_op.id );
							if( it_map == m_id_listener_map.end() ) {
								_ListenerVector* listeners = new _ListenerVector();
								m_id_listener_map[id] = listeners ;
								WAWO_LOG_DEBUG( "dispatcher_abstract", "register evt: %d, listener: 0x%p, dispatcher: 0x%p", id, listener.Get(), this ) ;
								listeners->push_back( listener );
							} else {
								_ListenerVector*& listeners = it_map->second;
#ifdef _DEBUG
								_MyListenerIterator it_listener = listeners->begin();
								while( it_listener != listeners->end() ) {
									if( (*it_listener) == listener ) {
										WAWO_LOG_DEBUG( "dispatcher_abstract", "multi-time register evt: %d, listener: 0x%p, dispatcher: 0x%p", id, listener.Get(), this ) ;
										WAWO_ASSERT( !"multi-time listener register" );
									}
									++it_listener;
								}
								WAWO_ASSERT( it_listener == listeners->end() );
#endif
								listeners->push_back( listener );
							}
						}
						break;
					case OP_REMOVE_ID_LISTENER:
						{

							WAWO_ASSERT( listener_op.id > 0 );
							WAWO_ASSERT( listener != NULL );

							_MyIdListenerIterator it_map = m_id_listener_map.find( id );
							if( it_map == m_id_listener_map.end() ) {
								WAWO_LOG_WARN("Dispatcher_Abstract", "[action remove] no listener found for %d", id );
								break ;
							}
							WAWO_ASSERT( it_map != m_id_listener_map.end() );

							_ListenerVector*& listeners = it_map->second;

							_MyListenerIterator it_listener = listeners->begin();
							while( it_listener != listeners->end() ) {
								if( (*it_listener) == listener ) {
									WAWO_LOG_DEBUG( "dispatcher_abstract", "unregister evt: %d, listener: 0x%p, dispatcher: 0x%p", id, (*it_listener).Get(), this ) ;
									it_listener = listeners->erase(it_listener);
								} else {
									++it_listener;
								}
							}
						}
						break;
					case OP_REMOVE_ID_ALL_LISTENER:
						{
							_MyIdListenerIterator it_map = m_id_listener_map.find( id );

							if( it_map != m_id_listener_map.end() ) {
								_ListenerVector*& listeners = it_map->second;
								_MyListenerIterator it_listener = listeners->begin();

								while( it_listener != listeners->end() ) {
									ListenerOp op(OP_REMOVE_ID_LISTENER, id, *it_listener );
									m_listener_ops.push(op);
									++it_listener;
								}
							}
						}
						break;
					case OP_REMOVE_LISTENER_ALL_ID:
						{
							_MyIdListenerIterator it = m_id_listener_map.begin();

							while( it != m_id_listener_map.end() ) {
								if( it->second->size() ) {
									ListenerOp op(OP_REMOVE_ID_LISTENER, it->first, listener );
									m_listener_ops.push(op);
								}
								++it;
							}
						}
						break;
					default:
						{
							WAWO_THROW_EXCEPTION("INVALID OP CODE FOR DISPATCHER");
						}
				}
			}
		}

		void Dispatch( WAWO_REF_PTR<E> const& evt, bool const& trigger = true ) {

			WAWO_ASSERT( evt->GetId() > 0 );

#ifdef USE_SHARED_MUTEX
			SharedLockGuard _lg(m_id_listener_map_mutex) ;
#else
			LockGuard<_MyIdListenerMapMutex> _lg(m_id_listener_map_mutex);
#endif

			if( !m_listener_ops.empty() ) {
#ifdef USE_SAHRED_MUTEX
				SharedUpgradeToLockGuard _lg(m_id_listener_map_mutex) ;
#endif
				_ProcessListenerOp();
			}

			_MyIdListenerIterator it = m_id_listener_map.find( evt->GetId() );
			if( it == m_id_listener_map.end() ) {
				WAWO_LOG_WARN( "Dispatcher_Abstract", "[action dispatch]no listener found for: %d", evt->GetId() ) ;
				return ;
			}

			_ListenerVector*& listeners = it->second ;
			_MyListenerIterator it_begin = listeners->begin();

			while( it_begin != listeners->end() ) {
				(*it_begin)->OnEvent(evt);
				++it_begin;
			}
		}
	};


	template <class E>
	class RaiseTask :
		public wawo::task::Task_Abstract
	{
		typedef Dispatcher_Abstract<E> _MyDispatcherT ;
	private:
		WAWO_REF_PTR<_MyDispatcherT> m_dp;
		WAWO_REF_PTR<E> m_evt;
	public:
		explicit RaiseTask( WAWO_REF_PTR<_MyDispatcherT> const& dp, WAWO_REF_PTR<E> const& e, wawo::task::Priority const& priority = wawo::task::P_NORMAL ):
			Task_Abstract(priority),
			m_dp(dp),
			m_evt(e)
		{
		}

		~RaiseTask() {
		}

		bool Isset() {
			return (m_dp != NULL)&& (m_evt != NULL);
		}

		void Reset() {
			WAWO_ASSERT( m_dp != NULL );
			WAWO_ASSERT( m_evt != NULL );

			m_dp = NULL;
			m_evt = NULL;
		}

		void Run() {
			WAWO_ASSERT( Isset() );
			m_dp->Trigger(m_evt) ;
		}

		void Cancel() {
			//nothing to do now...
		}
	};

	template <class E>
	class PostTask:
		public wawo::task::Task_Abstract
	{
		typedef Listener_Abstract<E> _MyListenerT ;
	private:
		WAWO_REF_PTR<_MyListenerT> m_l;
		WAWO_REF_PTR<E> m_evt;
	public:
		explicit PostTask( WAWO_REF_PTR<_MyListenerT> const& l, WAWO_REF_PTR<E> const& e, wawo::task::Priority const& priority = wawo::task::P_NORMAL ):
			Task_Abstract(priority),
			m_l(l),
			m_evt(e)
			{
			}

		~PostTask() {
		}

		bool Isset() {
			return (m_l != NULL)&& (m_evt != NULL);
		}

		void Reset() {
			WAWO_ASSERT( m_l != NULL );
			WAWO_ASSERT( m_evt != NULL );

			m_l = NULL;
			m_evt = NULL;
		}

		void Run() {
			WAWO_ASSERT( Isset() );
			m_l->OnEvent(m_evt) ;
		}

		void Cancel() {
			//nothing to do now...
		}
	};

}}}
#endif
