#ifndef _WAWO_EVENT_TRIGGER_HPP
#define _WAWO_EVENT_TRIGGER_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

#include <map>
#include <vector>

namespace wawo {

	static std::atomic<long> s_event_handler_id{ 0 };
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
		_Callable _callee;

		template<class... Args>
		void call(Args&&... args) {
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

		void unbind(int const& handler_id) {
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

		template<class _Lambda>
		int bind(int const& id, _Lambda&& lambda) {
			typedef std::remove_reference<_Lambda>::type lambda_t;
			WWRP<event_handler_base> handler = make_event_handler<lambda_t>(std::forward<lambda_t>(lambda));
			_insert_into_map( id, handler );
			return handler->id;
		}

		template<class handler_func_t, class _Lambda
			, class = typename std::enable_if<std::is_convertible<_Lambda, handler_func_t>::value>::type>
		int bind(int const& id, _Lambda&& lambda) {
			typedef std::remove_reference<_Lambda>::type lambda_t;
			WWRP<event_handler_base> handler = make_event_handler<handler_func_t>(std::forward<lambda_t>(lambda));
			_insert_into_map(id, handler);
			return handler->id;
		}

		template<class handler_func_t, class _Fx, class... _Args>
		int bind(int const& id, _Fx&& _func, _Args&&... _args) {
			WWRP<event_handler_base> handler = make_event_handler<handler_func_t>(std::bind(std::forward<_Fx>(_func), std::forward<_Args>(_args)...));
			_insert_into_map(id, handler);
			return handler->id;
		}

		template<class _Callable, class... _Args>
		void invoke(int id, _Args&&... _args) {
			typename event_map_t::iterator it = m_handlers.find(id);
			if (it != m_handlers.end()) {
				handler_vector_t::iterator it_handler = it->second.begin() ;
				while (it_handler != it->second.end()) {
					WWRP<event_handler<_Callable>> callee = wawo::dynamic_pointer_cast<event_handler<_Callable>>( *(it_handler++) );
					WAWO_ASSERT(callee != NULL);
					callee->call(std::forward<_Args>(_args)...);
				}
			}
		}
	};
}
#endif