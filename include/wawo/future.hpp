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
		private event_trigger
	{

	protected:
		enum future_event {
			E_COMPLETE
		};

		typedef std::function<void(WWRP< future<T> > const& f)> fn_operation_complete;
		typedef std::vector<int> listener_handler_vector;

		enum state {
			S_IDLE, //wait to operation
			S_CANCELLED, //operation cancelled
			S_SUCCESS, //operation done
			S_FAILURE //operation failed
		};

		mutex m_mutex;
		condition m_cond;
		listener_handler_vector m_handlers;
		WWSP<wawo::exception> m_exception;
		
		std::atomic<T> m_v;
		std::atomic<state> m_state;

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
			for( size_t i=0;i<m_handlers.size();++i) {
				event_trigger::unbind(m_handlers[i]);
			}
			m_handlers.clear();
		}
	public:
		future():
			m_v(T()),
			m_state(S_IDLE)
		{}

		virtual ~future()
		{}

		T get()
		{
			wait();
			return m_v.load(std::memory_order_acquire);
		}

		template <class _Rep, class _Period>
		T get(std::chrono::duration<_Rep, _Period> const& dur)
		{
			wait_for<_Rep,_Period>(dur);
			return m_v.load(std::memory_order_acquire);
		}

		void wait()
		{
			while (m_state.load(std::memory_order_acquire) == S_IDLE)
			{
				unique_lock<mutex> ulk(m_mutex);
				m_cond.wait(ulk);
			}
		}

		template <class _Rep, class _Period>
		void wait_for(std::chrono::duration<_Rep, _Period> const& dur)
		{
			if (m_state.load(std::memory_order_acquire) == S_IDLE) {
				unique_lock<mutex> ulk(m_mutex);
				m_cond.wait_for<_Rep, _Period>(ulk, dur);
			}
		}

		inline bool is_done() const {
			return m_state.load(std::memory_order_acquire) != S_IDLE;
		}

		inline bool is_success() const {
			return m_state.load(std::memory_order_acquire) == S_SUCCESS;
		}

		inline bool is_failure() const {
			return m_state.load(std::memory_order_acquire) == S_FAILURE;
		}

		WWSP<wawo::exception> cause() const {
			return m_exception;
		}

		bool cancel()
		{
			lock_guard<mutex> lg(m_mutex);
			if ( !m_state.compare_exchange_strong(S_IDLE, S_CANCELLED, std::memory_order_acq_rel)) {
				return false;
			}
			WAWO_ASSERT(m_state.load(std::memory_order_acquire) == S_CANCELLED);

			_notify_waiter();
			_notify_listeners();
			return true;
		}

		inline bool is_cancelled()
		{
			return m_state.load(std::memory_order_acquire) == S_CANCELLED;
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
	public:
		void set_success(T const& v) {
			lock_guard<mutex> lg(m_mutex);
			future<T>::state s = future<T>::S_IDLE;
			int ok = future<T>::m_state.compare_exchange_strong( s, future<T>::S_SUCCESS, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);

			T t = T();
			ok = future<T>::m_v.compare_exchange_strong(t, v, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);
			(void)ok;

			future<T>::_notify_waiter();
			future<T>::_notify_listeners();
		}

		void set_failure(WWSP<wawo::promise_exception> const& e) {
			lock_guard<mutex> lg(m_mutex);
			int ok = future<T>::m_state.compare_exchange_strong(future<T>::S_IDLE, future<T>::S_FAILURE, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);
			(void)ok;

			ok = future<T>::m_exception = e;

			future<T>::_notify_waiter();
			future<T>::_notify_listeners();
		}
	};
}

#endif