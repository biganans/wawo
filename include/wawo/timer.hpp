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

	/*
	 * @note
	 * it is a pure impl for stopwatch
	 * do not make your business depend on timer's life cycle management
	 *
	 * expire == zero() mean stoped

	 * @design consideration
	 * self managed thread update
	 * non self managed thread update

	 * @impl consideration
	 * 1,start new timer on timer thread is available [circle lock check and etc]
	 * 2,timer thread waken up notification on the following situation
	 *		a), new timer wait exceed current wait time
	 *		b), new timer wait less than current wait time
	 * 3,current timer thread should be terminated if no new timer started
	 * 4,must launch new thread when current thread is not running at the moment of new timer planed
	 * 5,timer thread could be interrupted at any given time [ie, interrupt wait state when process exit]
	 * 
	 */

	typedef std::chrono::steady_clock::duration timer_duration_t;
	typedef std::chrono::steady_clock::time_point timer_timepoint_t;
	typedef std::chrono::steady_clock timer_clock_t;

	const timer_timepoint_t _TIMER_TP_INFINITE = timer_timepoint_t() + timer_duration_t(~0);

	class timer:
		public wawo::ref_base
	{
		typedef std::function<void(WWRP<timer> const&, WWRP<wawo::ref_base> const& )> _fn_timer_t;
		_fn_timer_t callee;
		WWRP<wawo::ref_base> cookie;
		timer_duration_t delay;
		timer_timepoint_t expire;

		friend bool operator < (WWRP<timer> const& l, WWRP<timer> const& r);
		friend bool operator > (WWRP<timer> const& l, WWRP<timer> const& r);
		friend bool operator == (WWRP<timer> const& l, WWRP<timer> const& r);

		friend struct timer_less;
		friend struct timer_greater;
		friend class timer_manager;
	public:
		template <class dur, class _Fx, class... _Args>
		timer(dur const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args):
			callee(std::forward<_fn_timer_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)..., std::placeholders::_1, std::placeholders::_2))),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t())
		{
		}

		template <class dur, class _Callable
			, class=typename std::enable_if<std::is_convertible<_Callable, _fn_timer_t>::value>::type>
		timer(dur const& delay_, WWRP<wawo::ref_base> const& cookie_, _Callable&& callee_ ):
			callee(std::forward<std::remove_reference<_fn_timer_t>::type>(callee_)),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t())
		{
			static_assert(std::is_class<std::remove_reference<_Callable>>::value, "_Callable must be lambda or std::function type");
		}
	};

	inline bool operator < (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->expire < r->expire;
	}

	inline bool operator > (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->expire > r->expire;
	}

	inline bool operator == (WWRP<timer> const& l, WWRP<timer> const& r) {
		return l->expire == r->expire;
	}

	struct timer_less
	{
		inline bool operator()(WWRP<timer> const& l, WWRP<timer> const& r)
		{
			return l->expire < r->expire;
		}
	};

	struct timer_greater
	{
		inline bool operator()(WWRP<timer> const& l, WWRP<timer> const& r)
		{
			return l->expire > r->expire;
		}
	};

	class timer_manager
	{
		typedef std::queue<WWRP<timer>> _timer_queue;
		typedef wawo::binary_heap< WWRP<timer>, wawo::timer_less > _timer_heaper_t;

		wawo::mutex m_mutex;
		wawo::condition_variable m_cond;
		WWRP<wawo::thread> m_th;
		WWRP<_timer_heaper_t> m_heap;
		_timer_queue m_tq;

		timer_timepoint_t m_nexpire;
		bool m_has_own_run_th;
		bool m_th_break;
		bool m_in_wait;
		
		inline void __check_th_and_wait() {
			WAWO_ASSERT(m_has_own_run_th);

			//interrupt wait state
			if (m_in_wait) {
				WAWO_ASSERT(m_th_break == false);
				m_cond.notify_one();
				return;
			}

			if (m_th_break == true) {
				WAWO_ASSERT(m_in_wait == false);
				m_th = wawo::make_ref<wawo::thread>();
				WAWO_ALLOC_CHECK(m_th, sizeof(wawo::thread));
				int rt = m_th->start(&timer_manager::_run, this);
				WAWO_CONDITION_CHECK(rt == wawo::OK);
				m_th_break = false;
			}
		}

	public:
		timer_manager( bool ownth = true ):
			m_has_own_run_th(ownth),
			m_th_break(true),
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
			WAWO_WARN("[timer]cancel task count: %u", m_heap->size());
			while (!m_heap->empty()) {
				m_heap->pop();
			}
		}

		void start(WWRP<timer> const& t) {
			lock_guard<mutex> lg(m_mutex);
			WAWO_ASSERT(t != NULL);
			WAWO_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)));
			WAWO_ASSERT(t->expire == timer_timepoint_t());
			m_tq.push(t);
			if (m_has_own_run_th) {
				__check_th_and_wait();
			}
		}

		void start(WWRP<timer>&& t) {
			lock_guard<mutex> lg(m_mutex);
			WAWO_ASSERT(t != NULL);
			WAWO_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)) );
			WAWO_ASSERT(t->expire == timer_timepoint_t());
			m_tq.push(std::forward<WWRP<timer>>(t));
			if (m_has_own_run_th) {
				__check_th_and_wait();
			}
		}

		void _run() {
			WAWO_ASSERT(m_th != NULL);
			{
				unique_lock<mutex> ulg(m_mutex);
				WAWO_ASSERT(m_th != NULL);
				WAWO_ASSERT(m_th_break == false);
			}
			while (true) {
				update(m_nexpire);
				unique_lock<mutex> ulg(m_mutex);
				if (m_tq.size() == 0) {
					//exit this thread
					if (m_nexpire == _TIMER_TP_INFINITE) {
						m_th_break = true;
						break;
					} else {
						m_in_wait = true;
						m_cond.wait_until<timer_clock_t, timer_duration_t>(ulg, m_nexpire);
						m_in_wait = false;
					}
				}
			}
		}

		/*
		 * @note
		 * 1, if m_tq is not empty, check one by one ,
		 *		a) OP_START -> tm update expire and state, push heap, pop from tq
		 *		b) OP_STOP -> set expire to timepoint::zero(), pop from tq
		 * 2, check the first heap element and see whether it is expired
		 *		a) if expire == timepoint::zero(), ignore and pop, continue
		 *		b) if expire time reach, call callee, check REPEAT and push to tq if necessary, pop
		 * @param nexpire, next expire timepoint
		 *		a) updated on every frame when heap get updated (on both pop and push operation)
		 */
		inline void update(timer_timepoint_t& nexpire) {
			{
				unique_lock<mutex> ulg(m_mutex);
				while (!m_tq.empty()) {
					WWRP<timer>& tm = m_tq.front();
					WAWO_ASSERT(tm->delay.count() >= 0);
					tm->expire = timer_clock_t::now() + tm->delay;
					m_heap->push(std::move(tm));
					m_tq.pop();
				}
			}
			while (!m_heap->empty()) {
				WWRP<timer>& tm = m_heap->front();
				WAWO_ASSERT(tm->expire != timer_timepoint_t());
				WAWO_ASSERT(tm->expire != _TIMER_TP_INFINITE);
				
				if(timer_clock_t::now() < tm->expire) {
					nexpire = tm->expire;
					goto _recalc_nexpire;
				} else {
					tm->callee(tm, tm->cookie);
					m_heap->pop();
				}
			}
			//wait infinite
			nexpire = timer_timepoint_t() + timer_duration_t(~0);
		_recalc_nexpire:
			{//double check, and recalc wait time
				unique_lock<mutex> ulg(m_mutex);
				if (m_tq.size() != 0) {
					nexpire = timer_clock_t::now();
				}
			}
		}
	};

	class global_timer_manager:
		public singleton<timer_manager>
	{};
}
#endif