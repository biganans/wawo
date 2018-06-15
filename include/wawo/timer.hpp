#ifndef _WAWO_TIMER_HPP_
#define _WAWO_TIMER_HPP_

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/singleton.hpp>
#include <wawo/heap.hpp>

#include <wawo/mutex.hpp>
#include <wawo/thread.hpp>

#include <functional>
#include <chrono>
#include <queue>

namespace wawo {

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

	class timer : public wawo::ref_base
	{
		typedef std::function<void(WWRP<wawo::ref_base> const&, WWRP<timer> const&)> _timer_fx_t;
	public:
		WWRP<wawo::ref_base> cookie;
		_timer_fx_t callee;
		std::chrono::nanoseconds delay;
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> expire;
		timer_state s;
		timer_type t;

		template <class _Fx, class... _Args>
		timer(std::chrono::nanoseconds const& delay_, bool repeat, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args ):
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)..., std::placeholders::_1, std::placeholders::_2))),
			delay(delay_),
			expire(std::chrono::steady_clock::now()+delay),
			s(S_IDLE),
			t( repeat?T_REPEAT:T_ONESHOT)
		{
		}

		template <class _Fx, class... _Args>
		timer(std::chrono::nanoseconds const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args):
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)..., std::placeholders::_1, std::placeholders::_2))),
			delay(delay_),
			expire(std::chrono::steady_clock::now() + delay),
			s(S_IDLE),
			t(T_ONESHOT)
		{
		}

		template <class _Fx
			, class = typename std::enable_if<std::is_convertible<_Fx, _timer_fx_t>::value>::type>
			timer(std::chrono::nanoseconds const& delay_, bool repeat, WWRP<wawo::ref_base> const& cookie_, _Fx&& func) :
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(func)),
			delay(delay_),
			expire(std::chrono::steady_clock::now() + delay),
			s(S_IDLE),
			t(repeat? T_REPEAT : T_ONESHOT)
		{
		}

		template <class _Fx
			, class=typename std::enable_if<std::is_convertible<_Fx,_timer_fx_t>::value>::type>
		timer(std::chrono::nanoseconds const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func):
			cookie(cookie_),
			callee(std::forward<_timer_fx_t>(func)),
			delay(delay_),
			expire(std::chrono::steady_clock::now() + delay),
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

	class timer_manager
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

		wawo::mutex m_mutex;
		wawo::condition_variable m_cond;
		WWRP<wawo::thread> m_th;
		WWRP<_timer_heaper_t> m_heap;

		wawo::spin_mutex m_mutex_tq;
		_timer_queue m_tq;

		bool m_has_own_run_th;
		bool m_th_break;
		bool m_in_callee_loop;
		bool m_in_wait;

		void _check_start() {
			WAWO_ASSERT(m_has_own_run_th);
			if (m_in_callee_loop) { return; }//same thread...skip lock
			lock_guard<mutex> lg(m_mutex);
			if (m_th != NULL && m_th_break != true) {
				//interrupt wait state
				if (m_in_wait) {
					m_cond.notify_one();
				}
				return;
			}

			m_th = wawo::make_ref<wawo::thread>();
			WAWO_ALLOC_CHECK(m_th, sizeof(wawo::thread));
			int rt = m_th->start(&timer_manager::_run, this);
			WAWO_CONDITION_CHECK(rt == wawo::OK);
			m_th_break = false;
		}

	public:
		timer_manager(bool has_own_th = true ):
			m_has_own_run_th(has_own_th),
			m_th_break(true),
			m_in_callee_loop(false),
			m_in_wait(false)
		{
			m_heap = wawo::make_ref<_timer_heaper_t>();
		}

		~timer_manager()
		{
			if (m_th != NULL) {
				m_th->interrupt();
				m_th->join();
			}
			while (!m_heap->empty()) {
				m_heap->pop();
			}
		}

		void start(WWRP<timer> const& t) {
			{
				lock_guard<spin_mutex> lg(m_mutex_tq);
				WAWO_ASSERT(t != NULL);
				WAWO_ASSERT(t->delay.count() >= 0);
				m_tq.push({ OP_ADD,t });
			}

			if (m_has_own_run_th){
				_check_start();
			}
		}

		void stop(WWRP<timer> const& t) {
			lock_guard<spin_mutex> lg(m_mutex_tq);
			WAWO_ASSERT(t != NULL);
			m_tq.push({ OP_CANCEL,t });
			WAWO_ASSERT(m_th != NULL);
		}

		void _run() {
			WAWO_ASSERT(m_th != NULL);
			while (1) {
				unique_lock<mutex> ulg(m_mutex);
				std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> nexpire;
				m_in_callee_loop = true;
				update(nexpire);
				m_in_callee_loop = false;
				if ( nexpire.time_since_epoch().count() > 0) {
					m_in_wait = true;
					m_cond.wait_until<std::chrono::steady_clock, std::chrono::nanoseconds>(ulg, nexpire );
					m_in_wait = false;
				} else {
					m_th_break = true;
					break;
				}
			}
			m_heap->reserve(4096);
		}

		inline void update(std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>& nexpire) {
			while (true) {
				{
					lock_guard<spin_mutex> _lgtq(m_mutex_tq);
					std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now = std::chrono::steady_clock::now();
					while (!m_tq.empty()) {
						timer_& t = m_tq.front();
						WAWO_ASSERT(t._timer->delay.count() >= 0);
						if (t._op == OP_CANCEL) {
							WAWO_ASSERT(t._timer->s == timer_state::S_STARTED || t._timer->s == timer_state::S_EXPIRED);
							t._timer->s = timer_state::S_CANCELED;
						}
						else {
							if (t._timer->s != timer_state::S_CANCELED) {
								t._timer->s = timer_state::S_STARTED;
								m_heap->push(std::move(t._timer));
							}
						}
						m_tq.pop();
					}

					if (m_heap->empty()) {
						return;
					}
				}
				while (!m_heap->empty()) {
					WWRP<timer>& t = m_heap->front();
					const std::chrono::nanoseconds tdiff = (std::chrono::steady_clock::now() - t->expire);
					if (tdiff.count() >= 0) {
						switch (t->s)
						{
						case timer_state::S_STARTED:
						{
							t->s = timer_state::S_EXPIRED;
							t->callee(t->cookie, t);
							if (t->t == timer_type::T_REPEAT) {
								t->expire = std::chrono::steady_clock::now() + t->delay;
								lock_guard<spin_mutex> __lg(m_mutex_tq);
								m_tq.push({ OP_ADD, t });
							}
						}
						break;
						case timer_state::S_CANCELED:
						{
							t->callee(t->cookie, t);
						}
						break;
						default:
						{}
						break;
						}
						m_heap->pop();
					} else {
						nexpire = t->expire;
						return;
					}
				}
				try {
					wawo::this_thread::__interrupt_check_point();
				} catch (...) {
					return;
				}
			}
		}
	};

	class global_timer_manager:
		public singleton<timer_manager>
	{};
}
#endif