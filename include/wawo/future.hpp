#ifndef _WAWO_FUTURE_HPP
#define _WAWO_FUTURE_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/event_trigger.hpp>


#include <wawo/thread/condition.hpp>
#include <wawo/thread/mutex.hpp>

namespace wawo {

	using namespace wawo::thread;


	class promise_exception:
		public exception
	{
		promise_exception( int const& code, std::string const& reason ) :
			exception(code, reason.c_str(), __FILE__, __LINE__, __FUNCTION__)
		{
		}
	};

	template <typename T>
	class future:
		public ref_base,
		event_trigger
	{
		enum future_event {
			E_COMPLETE
		};

		typedef std::function<void(WWRP< future<T> > const& f)> fn_operation_complete;
		typedef std::vector<int> listener_handler_vector;

		enum state {
			S_IDLE, //wait to operation
			S_CANCELLED, //operation cancelled
			S_SUCCESS, //operation done
			S_FAILED //operation failed
		};

		mutex m_mutex;
		condition m_cond;
		T m_v;
		listener_handler_vector m_handlers;
		WWSP<wawo::exception> m_failed_exception;
		state m_state;

		inline bool _id_done() {
			return m_state == S_DONE;
		}

	protected:
		inline void _notify_waiter() {
			return m_cond.notify_all();
		}
		inline void _notify_listeners()
		{
			if (m_handlers.size() == 0) {
				return;
			}
			WWRP<future<T>>(this);
			event_trigger::invoke<fn_operation_complete>(E_COMPLETE, WWRP<future<T>>(this));
			for( int i=0;i<m_handlers.size();++i) {
				event_trigger::unbind(m_handlers[i]);
			}
			m_handlers.clear();
		}
	public:
		future():
			m_state(S_IDLE)
		{}

		virtual ~future()
		{}

		T get()
		{
			unique_lock<mutex> ulk(m_mutex);
			while (m_state == S_IDLE)
			{
				m_cond.wait(ulk);
			}
			return m_v;
		}

		template <class _Rep, class _Period>
		T get(std::chrono::duration<_Rep, _Period> const& dur)
		{
			unique_lock<mutex> ulk(m_mutex);
			if (m_state == S_IDLE)
			{
				m_cond.wait_for<_Rep,_Period>(ulk,dur);
			}
			return m_v;
		}

		void wait()
		{
			unique_lock<mutex> ulk(m_mutex);
			while (m_state == S_IDLE)
			{
				m_cond.wait(ulk);
			}
		}

		template <class _Rep, class _Period>
		void wait_for(std::chrono::duration<_Rep, _Period> const& dur)
		{
			unique_lock<mutex> ulk(m_mutex);
			if (m_state == S_IDLE)
			{
				m_cond.wait_for<_Rep, _Period>(ulk, dur);
			}
		}

		//
		inline bool is_done() const {
			lock_guard<mutex> lg(m_mutex);
			return m_state != S_IDLE;
		}

		inline bool is_success() const {
			lock_guard<mutex> lg(m_mutex);
			return m_state == S_SUCCESS;
		}

		inline bool is_failed() const {
			lock_guard<mutex> lg(m_mutex);
			return m_state == S_FAILED;
		}

		WWSP<wawo::exception>

		bool cancel()
		{
			lock_guard<mutex> lg(m_mutex);
			if (m_state != S_IDLE)
			{
				return false;
			}
			m_state = S_CANCELLED;
			_notify_waiter();
			_notify_listeners();
			return true;
		}

		inline bool is_cancelled()
		{
			lock_guard<mutex> lg(m_mutex);
			return m_state == S_CANCELLED;
		}

		template<class _Lambda
			, class = typename std::enable_if<std::is_convertible<_Lambda, fn_operation_complete>::value>::type>
		int add_listener(_Lambda&& lambda)
		{
			lock_guard<mutex> lg(m_mutex);
			int id = event_trigger::bind<fn_operation_complete>(E_COMPLETE, std::forward<_Lambda>(lambda));
			m_handlers.push_back(id);
			if( is_done() ) {
				_notify_listeners();
			}
			return id;
		}

		template<class _Fx, class... _Args>
		int add_listener(_Fx&& _func, _Args&&... _args)
		{
			lock_guard<mutex> lg(m_mutex);
			int id = event_trigger::bind<fn_operation_complete>(E_COMPLETE, std::forward<_Fx>(_func), std::forward<_Args>(_args)...);
			m_handlers.push_back(id);
			if (is_done()) {
				_notify_listeners();
			}
			return id;
		}

		void remove_listener(int const& id)
		{
			lock_guard<mutex> lg(m_mutex);
			typename listener_handler_vector::iterator it = std::find(m_handlers.begin(), m_handlers.end(), id) ;
			if (it != m_handlers.end())
			{
				m_listeners.erase(it);
				event_trigger::unbind(id);
			}
		}
	};

	template <typename T>
	class promise:
		public future<T>
	{

	};
}

#endif