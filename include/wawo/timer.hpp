#ifndef _WAWO_TIMER_HPP_
#define _WAWO_TIMER_HPP_

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/singleton.hpp>
#include <wawo/heap.hpp>

#include <functional>
#include <chrono>
#include <queue>

#include <wawo/thread/mutex.hpp>
#include <wawo/thread/thread.hpp>

namespace wawo {
	
	using namespace wawo::thread;

	enum timer_event {
		E_EXPIRED,
		E_CANCEL
	};

	enum timer_state {
		S_IDLE,
		S_STARTED,
		S_EXPIRED,
		S_CANCELED
	};

	enum timer_type {
		T_ONESHOT,
		T_REPEAT
	};

	class timer
		: public wawo::ref_base
	{
		typedef std::function<void(WWRP<wawo::ref_base> const& cookie, timer_event)> _timer_fx_t;
	public:
		WWRP<wawo::ref_base> cookie;
		_timer_fx_t callee;
		std::chrono::nanoseconds delay;
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> expire;
		timer_state s;
		timer_type t;

		template <class _Fx, class... _Args>
		timer(std::chrono::nanoseconds const& delay_, bool repeat, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args) :
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::placeholders::_1, std::placeholders::_2, std::forward<_Args>(args)...))),
			delay(delay_),
			s(S_IDLE),
			t(T_REPEAT)
		{
		}

		template <class _Fx, class... _Args>
		timer(std::chrono::nanoseconds const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args):
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::placeholders::_1, std::placeholders::_2, std::forward<_Args>(args)...))),
			delay(delay_),
			s(S_IDLE),
			t(T_ONESHOT)
		{
		}

		template <class _Fx
			, class = typename std::enable_if<std::is_convertible<_Fx, _timer_fx_t>::value>::type>
			timer(std::chrono::nanoseconds const& delay_, WWRP<wawo::ref_base> const& cookie_, bool repeat, _Fx&& func) :
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(func)),
			delay(std::chrono::nanoseconds(delay_)),
			s(S_IDLE),
			t(T_REPEAT)
		{}

		template <class _Fx
			, class=typename std::enable_if<std::is_convertible<_Fx,_timer_fx_t>::value>::type>
		timer(std::chrono::nanoseconds const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func):
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(func)),
			delay(delay_),
			s(S_IDLE),
			t(T_ONESHOT)
		{}
	};

	inline bool operator < (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->delay < r->delay;
	}

	inline bool operator > (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->delay > r->delay;
	}

	inline bool operator == (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->delay == r->delay;
	}

	struct timer_less
	{
		inline bool operator()(WWRP<timer> const& l, WWRP<timer> const& r)
		{
			return l->delay < r->delay;
		}
	};

	struct timer_greater
	{
		inline bool operator()(WWRP<timer> const& l, WWRP<timer> const& r)
		{
			return l->delay > r->delay;
		}
	};

	class timer_manager :
		public wawo::singleton<timer_manager>
	{
		enum timer_op {
			OP_ADD,
			OP_CANCEL
		};

		struct timer_ {
			timer_op _op;
			WWRP<timer> _timer;
		};

		typedef std::queue<timer_> _timer_queue;
		typedef wawo::binary_heap< WWRP<timer>, wawo::timer_less > _timer_heaper_t;

	private:
		wawo::thread::mutex m_mutex;
		wawo::thread::condition_variable m_cond;
		WWRP<wawo::thread::thread> m_th;
		WWRP<_timer_heaper_t> m_heap;
		_timer_queue m_tq;
		bool m_th_break;
	public:
		timer_manager()
		{
			m_heap = wawo::make_ref<_timer_heaper_t>(10240);
		}

		~timer_manager()
		{
			if (m_th != NULL) {
				m_th->interrupt();
			}
			while (!m_heap->empty()) {
				m_heap->pop();
			}
		}

		void start(WWRP<timer> const& t) {
			lock_guard<mutex> lg(m_mutex);
			WAWO_ASSERT(t != NULL);

			m_tq.push({ OP_ADD,t });
			_check_start();
		}

		void cancel(WWRP<timer> const& t) {
			lock_guard<mutex> lg(m_mutex);
			WAWO_ASSERT(t != NULL);
			m_tq.push({ OP_CANCEL,t });
			WAWO_ASSERT(m_th != NULL);
		}

		void _run() {
			WAWO_ASSERT(m_th != NULL);

			while (!m_th_break) {
				unique_lock<mutex> ulg(m_mutex);
				m_th_break = m_tq.empty() && m_heap->empty();

				std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now = std::chrono::steady_clock::now();
				if (!m_tq.empty()) {
					timer_& t = m_tq.front();
					WAWO_ASSERT(t._timer->delay.count() >= 0);
					if (t._op == OP_CANCEL) {
						WAWO_ASSERT(t._timer->s == timer_state::S_STARTED);
						t._timer->s = timer_state::S_CANCELED;
					} else {
						t._timer->expire = now + t._timer->delay;
						t._timer->s = timer_state::S_STARTED;
						m_heap->push(t._timer);
					}

					m_tq.pop();
				}

				while (!m_heap->empty()) {
					WWRP<timer>& t = m_heap->front();
					std::chrono::nanoseconds tdiff = (std::chrono::steady_clock::now() - t->expire);
					int64_t idiff = tdiff.count();

					if (idiff >= 0) {
						switch (t->s)
						{
						case timer_state::S_STARTED:
						{
							t->callee(t->cookie,timer_event::E_EXPIRED);
							if (t->t == timer_type::T_REPEAT) {
								m_tq.push({ OP_ADD, t });
							}
						}
						break;
						case timer_state::S_CANCELED:
						{
							t->callee(t->cookie, timer_event::E_CANCEL);
						}
						break;
						default:
						{}
						break;
						}
						m_heap->pop();
					} else {
						m_cond.wait_until<std::chrono::steady_clock, std::chrono::nanoseconds>(ulg, t->expire);
					}
				}
			}
		}

		void _check_start() {
			if (m_th != NULL && m_th_break != true) {
				return;
			}
			m_th = wawo::make_ref<wawo::thread::thread>();
			WAWO_ALLOC_CHECK(m_th, sizeof(wawo::thread::thread));
			int rt = m_th->start(&timer_manager::_run, this );
			WAWO_CONDITION_CHECK(rt == wawo::OK);
			m_th_break = false;
		}
	};
}
#endif