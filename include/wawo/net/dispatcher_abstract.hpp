#ifndef _WAWO_NET_DISPATCHER_HPP_
#define _WAWO_NET_DISPATCHER_HPP_

#include <vector>
#include <queue>

#include <wawo/smart_ptr.hpp>

#include <wawo/thread/mutex.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/net/net_event.hpp>
#include <wawo/net/listener_abstract.hpp>
#include <wawo/task/scheduler.hpp>

namespace wawo { namespace net {

	using namespace wawo::thread;

	/* RULES
	 * 1, you must register the relevant evt id before your listener can receive your evt
	 * 2, you must unregister the relevant evt ids before your listener be deleted
	 * 3, break 1, your listener receive nothing (obviously)
	 * 4, break 2, behaviour undefined
	 */

	//E, event to dispatch, E must be subclass of event
	//L, Listener

	template <class E>
	class schedule_task;

	static std::atomic<u32_t> s_dp_auto_incre_id(0);

	template <class E>
	class dispatcher_abstract :
		virtual public wawo::ref_base
	{

	private:
		typedef schedule_task<E>	_schedule_task_t;
	public:
		typedef wawo::task::fn_lambda fn_lambda_t;
		typedef listener_abstract<E> _listener_t;
		typedef dispatcher_abstract<E> _dispatcher_t;

		enum ListenerOpCode {
			OP_ADD_ID_LISTENER,
			OP_REMOVE_ID_LISTENER,
			OP_REMOVE_LISTENER_ALL_ID,
			OP_REMOVE_ID_ALL_LISTENER
		};

		struct listener_op {
			WWRP<_listener_t> listener;
			u8_t op;
			u8_t id;
		};

		typedef std::queue<listener_op> _listener_op_queue;
		typedef std::vector< WWRP<_listener_t> > _listener_vector;

	private:
		spin_mutex m_id_listener_vector_mutex;
		_listener_vector m_id_listener_vector[NE_MAX];

		spin_mutex m_listener_ops_mutex;
		_listener_op_queue m_listener_ops;

		u32_t m_dp_id;
	public:
		dispatcher_abstract():
			m_listener_ops_mutex(),
			m_listener_ops(),
			m_dp_id(wawo::atomic_increment(&s_dp_auto_incre_id))
		{
		}

		virtual ~dispatcher_abstract() {}

		inline u32_t const& get_dp_id() const { return m_dp_id; }

		void register_listener( u8_t const& id, WWRP<_listener_t> const& listener, bool const& force_update = false ) {
			_add_listener_op(listener, OP_ADD_ID_LISTENER, id );
			if(WAWO_UNLIKELY(force_update) ) {
				lock_guard<spin_mutex> _lg(m_id_listener_vector_mutex);
				_process_listener_op();
			}
		}

		//don't use force_update !!!
		void unregister_listener(u8_t const& id, WWRP<_listener_t> const& listener, bool const& force_update = false ) {
			_add_listener_op(listener, OP_REMOVE_ID_LISTENER, id );
			if(WAWO_UNLIKELY(force_update) ) {
				lock_guard<spin_mutex> _lg(m_id_listener_vector_mutex);
				_process_listener_op();
			}
		}

		void unregister_listener( WWRP<_listener_t> const& listener, bool const& force_update = false ) {
			_add_listener_op(listener, OP_REMOVE_LISTENER_ALL_ID,0);
			if( WAWO_UNLIKELY(force_update) ) {
				lock_guard<spin_mutex> _lg(m_id_listener_vector_mutex);
				_process_listener_op();
			}
		}

		void unregister_listener(u8_t const& id, bool const& force_update = false) {
			_add_listener_op( OP_REMOVE_ID_ALL_LISTENER,id, NULL );
			if( WAWO_UNLIKELY(force_update) ) {
				lock_guard<spin_mutex> _lg(m_id_listener_vector_mutex);
				_process_listener_op();
			}
		}

		void trigger(WWRP<E> const& evt) {

			WAWO_ASSERT((evt->id >= 0) && evt->id< NE_MAX);
			lock_guard<spin_mutex> _lg(m_id_listener_vector_mutex);
			if ( WAWO_LIKELY(!m_listener_ops.empty())) {
				_process_listener_op();
			}
			u32_t lsize = m_id_listener_vector[evt->id].size();
			if ( WAWO_UNLIKELY(lsize == 0)) {
				WAWO_WARN("[dispatcher_abstract]no listener found for: %d", evt->id);
			}
			else {
				for (u32_t i = 0; i < lsize; ++i) {
					m_id_listener_vector[evt->id][i]->on_event(evt);
				}
			}
		}

		//return immediately , plan a dispatcher on task manager, and then call trigger
		inline void schedule( WWRP<E> const& evt, u8_t const& priority = wawo::task::P_NORMAL ) {
			WAWO_SCHEDULER->schedule( wawo::make_ref<_schedule_task_t>(WWRP< _dispatcher_t >(this), evt), priority );
		}

		inline void schedule( fn_lambda_t const& lambda, u8_t const& priority = wawo::task::P_NORMAL ) {
			WAWO_SCHEDULER->schedule(lambda, priority );
		}

		inline void oschedule( WWRP<E> const& evt, u32_t const& tag = 0 ) {
			WWRP<wawo::task::task_abstract> evt_task = wawo::make_ref<_schedule_task_t>(WWRP< _dispatcher_t >(this), evt);
			WWRP<wawo::task::sequencial_task> seq_task = wawo::make_ref<wawo::task::sequencial_task>(evt_task, ((tag == 0) ? m_dp_id : tag) );
			WAWO_SCHEDULER->schedule(seq_task);
		}

		inline void oschedule(fn_lambda_t const& lambda, u32_t const& tag = 0 ) {
			WWRP<wawo::task::task_abstract> lambda_task = wawo::make_ref<wawo::task::lambda_task>(lambda);
			WWRP<wawo::task::sequencial_task> seq_task = wawo::make_ref<wawo::task::sequencial_task>(lambda_task, ( (tag==0)?m_dp_id:tag) );
			WAWO_SCHEDULER->schedule(seq_task);
		}

	private:
		inline void _add_listener_op(WWRP<_listener_t> const& listener, u8_t const& code, u8_t const& id ) {
			lock_guard<spin_mutex> _ls( m_listener_ops_mutex );
			m_listener_ops.push({ listener,code,id });
		}
		void _process_listener_op() {
			lock_guard<spin_mutex> _ls( m_listener_ops_mutex );

			while( m_listener_ops.size() ) {

				listener_op listener_op = m_listener_ops.front();
				m_listener_ops.pop();

				u8_t const& op_code					= listener_op.op ;
				u8_t const& id						= listener_op.id;
				WWRP<_listener_t> const& listener	= listener_op.listener;

				WAWO_ASSERT( (id >= 0) && id<NE_MAX );

				switch( op_code ) {

					case OP_ADD_ID_LISTENER:
						{
							WAWO_ASSERT( listener != NULL );
#ifdef _DEBUG
							typename _listener_vector::iterator it = std::find(m_id_listener_vector[id].begin(), m_id_listener_vector[id].end(), listener);
							WAWO_ASSERT( it == m_id_listener_vector[id].end() );
#endif
							m_id_listener_vector[id].push_back( listener );
						}
						break;
					case OP_REMOVE_ID_LISTENER:
						{
							WAWO_ASSERT( listener != NULL );
							typename _listener_vector::iterator it = std::find( m_id_listener_vector[id].begin(), m_id_listener_vector[id].end(), listener);
							if (it != m_id_listener_vector[id].end()) {
								m_id_listener_vector[id].erase(it);
							}
						}
						break;
					case OP_REMOVE_LISTENER_ALL_ID:
						{
							for (u8_t i = 0; i < NE_MAX; ++i) {
								typename _listener_vector::iterator it = m_id_listener_vector[i].begin();
								while (it != m_id_listener_vector[i].end()) {
									if (*it == listener) {
										it = m_id_listener_vector[i].erase(it);
									}
									else {
										++it;
									}
								}
							}
						}
						break;
					case OP_REMOVE_ID_ALL_LISTENER:
						{
							m_id_listener_vector[id].clear();
						}
						break;
					default:
						{
							WAWO_THROW("INVALID OP CODE FOR DISPATCHER");
						}
				}
			}
		}
	};

	template <class E>
	class schedule_task :
		public wawo::task::task_abstract
	{
		typedef dispatcher_abstract<E> _dispatcher_t ;
	private:
		WWRP<_dispatcher_t> m_dp;
		WWRP<E> m_evt;
	public:
		explicit schedule_task( WWRP<_dispatcher_t> const& dp, WWRP<E> const& e):
			task_abstract(),
			m_dp(dp),
			m_evt(e)
		{
		}

		~schedule_task() {}
		void run() {
			WAWO_ASSERT( m_dp != NULL );
			m_dp->trigger(m_evt) ;
		}
	};
}}
#endif
