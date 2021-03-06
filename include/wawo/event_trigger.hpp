#ifndef _WAWO_EVENT_TRIGGER_HPP
#define _WAWO_EVENT_TRIGGER_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

#include <map>
#include <vector>

namespace wawo {

	static std::atomic<long> s_event_handler_id{0};
	class event_handler_base :public wawo::ref_base
	{
		friend class event_trigger;
	protected:
		long id;
	public:
		event_handler_base():
			id(wawo::atomic_increment(&s_event_handler_id))
		{}
	};

	template <class _Callable>
	class event_handler : public wawo::event_handler_base
	{
		friend class event_trigger;
		typename std::remove_reference<_Callable>::type _callee;

		template<class... Args>
		inline void call(Args&&... args) {
			_callee(std::forward<Args>(args)...);
		}
	public:
		event_handler(_Callable&& callee) :
			_callee(std::forward<_Callable>(callee))
		{}
	};

	template <class _Callable>
	inline WWRP<event_handler<_Callable>> make_event_handler(_Callable&& _func) {
		return wawo::make_ref<event_handler<_Callable>>(std::forward<_Callable>(_func));
	}

	class event_trigger
	{
		typedef std::vector< WWRP<wawo::event_handler_base> > handler_vector_t;
		typedef std::map<int, handler_vector_t > event_map_t;

		event_map_t m_handlers;
	public:
		event_trigger() :m_handlers() {}
		virtual ~event_trigger() {}

		inline void _insert_into_map( int const& id, WWRP<wawo::event_handler_base> const& h ) {
			event_map_t::iterator it = m_handlers.find(id);
			if (it != m_handlers.end()) {
				it->second.push_back(h);
				return;
			}
			handler_vector_t v;
			v.push_back(h);
			m_handlers.insert({id, v});
		}

		inline void unbind(long const& handler_id) {
			event_map_t::iterator it = m_handlers.begin();
			while ( it != m_handlers.end()) {
				handler_vector_t::iterator it_handlers = it->second.begin();
				while (it_handlers != it->second.end()) {
					if (handler_id == (*it_handlers)->id) {
						it->second.erase(it_handlers);
						return;
					}
				}
				++it;
			}
		}

		template<class _Callable>
		inline long bind(int const& evt_id, _Callable&& evt_callee ) {
			static_assert(std::is_class<std::remove_reference<_Callable>>::value, "_Callable must be lambda or std::function type");
			WWRP<event_handler_base> evt_handler = make_event_handler<_Callable>(std::forward<_Callable>(evt_callee));
			_insert_into_map(evt_id, evt_handler);
			return evt_handler->id;
		}

		template<class _Callable_Hint, class _Callable
			, class = typename std::enable_if<std::is_convertible<_Callable, _Callable_Hint>::value>::type>
		inline long bind(int const& evt_id, _Callable&& evt_callee) {
			static_assert(std::is_class<std::remove_reference<_Callable>>::value, "_Callable must be lambda or std::function type");
			WWRP<event_handler_base> evt_handler = make_event_handler<_Callable_Hint>(std::forward<std::remove_reference<_Callable>::type>(evt_callee));
			_insert_into_map(evt_id, evt_handler);
			return evt_handler->id;
		}

		template<class _Callable_Hint, class _Fx, class... _Args>
		inline long bind(int const& evt_id, _Fx&& _func, _Args&&... _args) {
			WWRP<event_handler_base> evt_handler = make_event_handler<_Callable_Hint>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			_insert_into_map(evt_id, evt_handler);
			return evt_handler->id;
		}

		template<class _Callable_Hint, class... _Args>
		void invoke(int id, _Args&&... _args) {
			typename event_map_t::iterator it = m_handlers.find(id);
			if (it != m_handlers.end()) {
				handler_vector_t::iterator it_handler = it->second.begin() ;
				while (it_handler != it->second.end()) {
					WWRP<event_handler<_Callable_Hint>> callee_handler = wawo::dynamic_pointer_cast<event_handler<_Callable_Hint>>( *(it_handler++) );
					WAWO_ASSERT(callee_handler != NULL);
					callee_handler->call(std::forward<_Args>(_args)...);
				}
			}
		}
	};
}
#endif