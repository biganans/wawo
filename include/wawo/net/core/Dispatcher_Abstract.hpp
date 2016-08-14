#ifndef _WAWO_NET_CORE_DISPATCHER_HPP_
#define _WAWO_NET_CORE_DISPATCHER_HPP_

#include <vector>
#include <queue>
#include <map>

#include <wawo/SmartPtr.hpp>

#include <wawo/thread/Mutex.hpp>
#include <wawo/log/LoggerManager.h>

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/core/IOTaskManager.hpp>


#ifdef _DEBUG
#define _DP_ADD_ID_LISTENER(map,id,listener) \
	do { \
		_MyIdListenerIterator it_map = map.find(id); \
		if (it_map == m_id_listener_map.end()) { \
			_ListenerVector* listeners = new _ListenerVector(); \
			map[id] = listeners; \
			listeners->push_back(listener); \
		} else { \
			_ListenerVector* const& listeners = it_map->second; \
			_MyListenerIterator it_listener = listeners->begin(); \
			while (it_listener != listeners->end()) { \
				if ((*it_listener) == listener) { \
					WAWO_THROW_EXCEPTION("multi-time listener register"); \
				} \
				++it_listener; \
			} \
			WAWO_ASSERT(it_listener == listeners->end()); \
			listeners->push_back(listener); \
		} \
	} while(0)
#else
	#define _DP_ADD_ID_LISTENER(map,id,listener) \
	do { \
		_MyIdListenerIterator it_map = map.find(id); \
		if (it_map == m_id_listener_map.end()) { \
			_ListenerVector* listeners = new _ListenerVector(); \
			map[id] = listeners; \
			listeners->push_back(listener); \
		} else { \
			_ListenerVector*& listeners = it_map->second; \
			listeners->push_back(listener); \
		} \
	} while(0)
#endif

#define _DP_REMOVE_ID_LISTENER(map,id,listener) \
do { \
	_MyIdListenerIterator it_map = map.find( id ); \
	if (it_map == map.end()) { \
		WAWO_WARN("[Dispatcher_Abstract][action remove] no listener found for %d", id); \
		break; \
	} \
	WAWO_ASSERT(it_map != map.end()); \
	_ListenerVector*& listeners = it_map->second; \
	_MyListenerIterator it_listener = listeners->begin(); \
	while (it_listener != listeners->end()) { \
		if ((*it_listener) == listener) { \
			it_listener = listeners->erase(it_listener); \
		} else { \
			++it_listener; \
		} \
	} \
} while (0)

//#define USE_SHARED_MUTEX_FOR_ID_LISTENER_MAP
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

	//template <class E>
	//class PostTask;

	template <class E>
	class ScheduleTask;

	static std::atomic<u32_t> s_dp_auto_incre_id(0);

	template <class E>
	class Dispatcher_Abstract :
		virtual public wawo::RefObject_Abstract
	{

	public:
		typedef wawo::task::LambdaFn LambdaFnT;
		typedef ScheduleTask<E>	_ScheduleTask;

		typedef Listener_Abstract<E> _ListenerT;
		typedef Dispatcher_Abstract<E> _DispatcherT;

		enum ListenerOpCode {
			OP_ADD_ID_LISTENER,
			OP_REMOVE_ID_LISTENER,
			OP_REMOVE_ID_ALL_LISTENER,
			OP_REMOVE_LISTENER_ALL_ID
		};

		struct ListenerOp {
			ListenerOpCode op:8; //default on
			int id:24;
			WWRP<_ListenerT> listener;

			ListenerOp( ListenerOpCode code , int const& id, WWRP<_ListenerT> const& l ):
				op(code),
				id(id),
				listener(l)
			{
			}

			bool operator == ( ListenerOp const& second ) {
				return (op == second.op)
						&& (id == second.id)
						&& (listener == second.listener)
					;
			}
		};

		typedef std::queue<ListenerOp> _ListenerOpQueue;
		typedef std::vector< WWRP<_ListenerT> > _ListenerVector;
		typedef typename _ListenerVector::iterator _MyListenerIterator;

		typedef std::map< int, _ListenerVector* > _IdListenerMap;
		typedef typename _IdListenerMap::iterator _MyIdListenerIterator;

	private:
		SpinMutex m_id_listener_map_mutex;
		_IdListenerMap m_id_listener_map;

		SpinMutex m_listener_ops_mutex;
		_ListenerOpQueue m_listener_ops;

		u32_t m_dp_id;
	public:
		Dispatcher_Abstract():
			m_id_listener_map_mutex(),
			m_id_listener_map(),
			m_listener_ops_mutex(),
			m_listener_ops(),
			m_dp_id(wawo::atomic_increment(&s_dp_auto_incre_id))
		{
			//WAWO_INFO("[Dispatcher_Abstract] Dispatcher_Abstract::Dispatcher_Abstract() get id: %u", m_dp_id );
		}

		virtual ~Dispatcher_Abstract() {
			_UnRegisterAll();
		}

		void _UnRegisterAll() {
			_ProcessListenerOp();
			LockGuard<SpinMutex> _lg( m_id_listener_map_mutex );
			_MyIdListenerIterator it = m_id_listener_map.begin();

			u32_t count = 0;
			while ( it != m_id_listener_map.end() ) {
				_MyIdListenerIterator it_to_unregister = it;
				++it;
				count += it_to_unregister->second->size();
				it_to_unregister->second->clear();
				WAWO_DELETE( it_to_unregister->second );
				m_id_listener_map.erase( it_to_unregister );
			}
		}

		void Register( int const& id, WWRP<_ListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_ADD_ID_LISTENER, id, listener );
			if( force_update ) {
				LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		//don't use force_update !!!
		void UnRegister( int const& id, WWRP<_ListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_REMOVE_ID_LISTENER, id, listener );
			if( force_update ) {
				LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		void UnRegister( WWRP<_ListenerT> const& listener, bool const& force_update = false ) {
			_AddListenerOp( OP_REMOVE_LISTENER_ALL_ID,-1,listener );
			if( force_update ) {
				LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		void UnRegister( int const& id, bool const& force_update = false) {
			_AddListenerOp( OP_REMOVE_ID_ALL_LISTENER,id, NULL );
			if( force_update ) {
				LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
				_ProcessListenerOp();
			}
		}

		void Trigger(WWRP<E> const& evt) {

			LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
			if (!m_listener_ops.empty()) {
				_ProcessListenerOp();
			}

			_MyIdListenerIterator it = m_id_listener_map.find(evt->GetId());
			if (it == m_id_listener_map.end()) {
				WAWO_WARN("[Dispatcher_Abstract][action dispatch]no listener found for: %d", evt->GetId());
				return;
			}

			_ListenerVector*& listeners = it->second;
			_MyListenerIterator it_begin = listeners->begin();

			while (it_begin != listeners->end()) {
				(*it_begin)->OnEvent(evt);
				++it_begin;
			}
		}

		//return immediately , plan a dispatcher on task manager, and then call Trigger
		inline void Schedule( WWRP<E> const& evt, wawo::task::Priority const& priority = wawo::task::P_NORMAL ) {
			WWRP<wawo::task::Task_Abstract> evt_task( new _ScheduleTask( WWRP< _DispatcherT >(this), evt ) );
			WAWO_NET_CORE_IO_TASK_MANAGER->Schedule( evt_task, priority );
		}

		inline void Schedule( LambdaFnT const& lambda_RUN, LambdaFnT const& lambda_CANCEL = nullptr, wawo::task::Priority const& priority = wawo::task::P_NORMAL ) {
			WWRP<wawo::task::Task_Abstract> lambda_task( new wawo::task::LambdaTask( lambda_RUN, lambda_CANCEL ) );
			WAWO_NET_CORE_IO_TASK_MANAGER->Schedule( lambda_task, priority );
		}

		//
		inline void OSchedule( WWRP<E> const& evt ) {
			WWRP<wawo::task::Task_Abstract> evt_task( new _ScheduleTask( WWRP< _DispatcherT >(this), evt ) );
			WWRP<wawo::task::SequencialTask> seq_task( new wawo::task::SequencialTask(evt_task , m_dp_id ));
			WAWO_NET_CORE_IO_TASK_MANAGER->Schedule( seq_task );
		}

		inline void OSchedule( LambdaFnT const& lambda_RUN, LambdaFnT const& lambda_CANCEL = nullptr ) {
			WWRP<wawo::task::Task_Abstract> lambda_task( new wawo::task::LambdaTask( lambda_RUN, lambda_CANCEL ) );
			WWRP<wawo::task::SequencialTask> seq_task( new wawo::task::SequencialTask(lambda_task, m_dp_id ));
			WAWO_NET_CORE_IO_TASK_MANAGER->Schedule( seq_task );
		}

		//inline void ForceUpdateListenerMap() {
		//	LockGuard<SpinMutex> _lg(m_id_listener_map_mutex);
		//	if( !m_listener_ops.empty() ) {
		//		_ProcessListenerOp();
		//	}
		//}
	private:
		inline void _AddListenerOp( ListenerOpCode const& code, int const& id, WWRP<_ListenerT> const& listener ) {
			LockGuard<SpinMutex> _ls( m_listener_ops_mutex );
			ListenerOp op (code,id,listener);
			m_listener_ops.push( op );
		}
		inline void _ProcessListenerOp() {
			LockGuard<SpinMutex> _ls( m_listener_ops_mutex );

			while( m_listener_ops.size() ) {

				ListenerOp listener_op = m_listener_ops.front();
				m_listener_ops.pop();

				ListenerOpCode const& op_code		= listener_op.op ;
				int const& id						= listener_op.id;
				WWRP<_ListenerT> const& listener	= listener_op.listener;

				switch( op_code ) {

					case OP_ADD_ID_LISTENER:
						{
							WAWO_ASSERT( listener_op.id > 0 );
							WAWO_ASSERT( listener != NULL );
							_DP_ADD_ID_LISTENER(m_id_listener_map,id,listener );
						}
						break;
					case OP_REMOVE_ID_LISTENER:
						{
							WAWO_ASSERT( id > 0 );
							WAWO_ASSERT( listener != NULL );
							_DP_REMOVE_ID_LISTENER( m_id_listener_map,id,listener );
						}
						break;
					case OP_REMOVE_ID_ALL_LISTENER:
						{
							_MyIdListenerIterator it_map = m_id_listener_map.find( id );
							_ListenerOpQueue tmp_Q;

							if( it_map != m_id_listener_map.end() ) {
								_ListenerVector*& listeners = it_map->second;
								_MyListenerIterator it_listener = listeners->begin();

								while( it_listener != listeners->end() ) {
									ListenerOp op(OP_REMOVE_ID_LISTENER, id, *it_listener );
									tmp_Q.push(op);
									++it_listener;
								}

								while (tmp_Q.size()) {
									ListenerOp op = tmp_Q.front();
									tmp_Q.pop();
									_DP_REMOVE_ID_LISTENER(m_id_listener_map, op.id, op.listener);
								}
							}
						}
						break;
					case OP_REMOVE_LISTENER_ALL_ID:
						{
							_MyIdListenerIterator it = m_id_listener_map.begin();
							_ListenerOpQueue tmp_Q;

							while( it != m_id_listener_map.end() ) {
								if( it->second->size() ) {
									ListenerOp op(OP_REMOVE_ID_LISTENER, it->first, listener );
									tmp_Q.push(op);
								}
								++it;
							}

							while (tmp_Q.size()) {
								ListenerOp op = tmp_Q.front();
								tmp_Q.pop();
								_DP_REMOVE_ID_LISTENER(m_id_listener_map,op.id,op.listener);
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
	};

	template <class E>
	class ScheduleTask :
		public wawo::task::Task_Abstract
	{
		typedef Dispatcher_Abstract<E> _MyDispatcherT ;
	private:
		WWRP<_MyDispatcherT> m_dp;
		WWRP<E> m_evt;
	public:
		explicit ScheduleTask( WWRP<_MyDispatcherT> const& dp, WWRP<E> const& e):
			Task_Abstract(),
			m_dp(dp),
			m_evt(e)
		{
		}

		~ScheduleTask() {
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
		WWRP<_MyListenerT> m_l;
		WWRP<E> m_evt;
	public:
		explicit PostTask( WWRP<_MyListenerT> const& l, WWRP<E> const& e, wawo::task::Priority const& priority = wawo::task::P_NORMAL ):
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
