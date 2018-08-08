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
		S_EXPIRED
	};

	enum timer_type {
		T_ONESHOT,
		T_REPEAT
	};

	/*
	 * @note
	 * it is a pure impl for stopwatch and repeatable stopwatch
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
	 * 4,must launch new thread when current thread is not running at the moment
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
		timer_type type;
		timer_state state;

		friend bool operator < (WWRP<timer> const& l, WWRP<timer> const& r);
		friend bool operator > (WWRP<timer> const& l, WWRP<timer> const& r);
		friend bool operator == (WWRP<timer> const& l, WWRP<timer> const& r);

		friend struct timer_less;
		friend struct timer_greater;
		friend class timer_manager;
	public:
		template <class dur, class _Fx, class... _Args>
		timer(dur const& delay_, bool repeat, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args ):
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)..., std::placeholders::_1, std::placeholders::_2))),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t()),
			type(repeat ? T_REPEAT : T_ONESHOT),
			state(S_IDLE)
		{
		}

		template <class dur, class _Fx, class... _Args>
		timer(dur const& delay_, WWRP<wawo::ref_base> const& cookie_, _Fx&& func, _Args&&... args):
			callee(std::forward<_timer_fx_t>(std::bind(std::forward<_Fx>(func), std::forward<_Args>(args)..., std::placeholders::_1, std::placeholders::_2))),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t()),
			type(T_ONESHOT),
			state(S_IDLE)
		{
		}

		template <class dur, class _Callable
			, class = typename std::enable_if<std::is_convertible<_Callable, _fn_timer_t>::value>::type>
			timer(dur const& delay_, bool repeat, WWRP<wawo::ref_base> const& cookie_, _Callable&& callee_) :
			callee(std::forward<std::remove_reference<_fn_timer_t>::type>(callee_)),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t()),
			type(repeat ? T_REPEAT : T_ONESHOT),
			state(S_IDLE)
		{
			static_assert(std::is_class<std::remove_reference<_Callable>>::value, "_Callable must be lambda or std::function type");
		}

		template <class dur, class _Callable
			, class=typename std::enable_if<std::is_convertible<_Callable, _fn_timer_t>::value>::type>
		timer(dur const& delay_, WWRP<wawo::ref_base> const& cookie_, _Callable&& callee_ ):
			callee(std::forward<std::remove_reference<_fn_timer_t>::type>(callee_)),
			cookie(cookie_),
			delay(delay_),
			expire(timer_timepoint_t()),
			type(T_ONESHOT),
			state(S_IDLE)
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
		enum timer_op_code {
			OP_START,
			OP_STOP
		};

		struct timer_op {
			timer_op_code code;
			WWRP<timer> tm;
		};

		typedef std::queue<timer_op> _timer_queue;
		typedef wawo::binary_heap< WWRP<timer>, wawo::timer_less > _timer_heaper_t;

		wawo::mutex m_mutex;
		wawo::condition_variable m_cond;
		WWRP<wawo::thread> m_th;
		WWRP<_timer_heaper_t> m_heap;

		wawo::spin_mutex m_mutex_tq;
		_timer_queue m_tq;

		std::thread::id m_th_id;

		timer_timepoint_t m_nexpire;
		bool m_has_own_run_th;
		bool m_th_break;
		bool m_in_wait;
		

		void __check_th() {
			WAWO_ASSERT(m_has_own_run_th);
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

		void __check_wait() {
			if (std::this_thread::get_id() == m_th_id) { return; }//same thread...skip lock

		}
	public:
		timer_manager( std::thread::id tid = std::thread::id() ):
			m_has_own_run_th(tid == std::thread::id()),
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
			WAWO_ASSERT(t->delay >= timer_duration_t(0) && (t->delay != timer_duration_t(~0)) );
			WAWO_ASSERT(t->expire == timer_timepoint_t());
			m_tq.push({ OP_START,t });

			if (m_has_own_run_th && ){
				__check_self_th();
			}
		}

		void stop(WWRP<timer> const& t) {
			lock_guard<spin_mutex> lg(m_mutex_tq);
			WAWO_ASSERT(t != NULL);
			m_tq.push({ OP_STOP,t });
			WAWO_ASSERT(m_th != NULL);
		}

		void _run() {
			WAWO_ASSERT(m_th != NULL);
			{
				unique_lock<mutex> ulg(m_mutex);
				WAWO_ASSERT(m_th_id == std::thread::id());
				m_th_id = std::thread::id();
			}
			while (true) {
				try {
					wawo::this_thread::__interrupt_check_point();
				} catch (...) {
					return;
				}
				update(m_nexpire);
				/*
				if (m_nexpire == timer_timepoint_t() + timer_duration_t(~0)) {

					wawo::lock_guard<wawo::spin_mutex> lgtq(m_mutex_tq);
					if(m_tq.empty())
					//no timer left
					m_th_id = std::thread::id();//clear tid
					m_th_break = true;
					break;
				}
				*/
				if (m_nexpire.time_since_epoch().count() != 0) {
					//double check
					unique_lock<mutex> ulg(m_mutex);
					if (m_tq.empty()) {
						m_in_wait = true;
						if (m_nexpire == _TIMER_TP_INFINITE) {
							m_cond.wait(ulg);
						} else {
							m_cond.wait_until<timer_clock_t, timer_duration_t>(ulg, m_nexpire);
						}
						m_in_wait = false;
					}
				}
			}
			m_heap->reserve(1024);
		}

		/*
		 * @note
		 * 1, if m_tq is not empty, check one by one ,
		 *		a) OP_START -> tm update expire and state, push heap, pop from tq
		 *		b) OP_STOP -> set expire to timepoint::zero(), pop from tq
		 * 2, check the first heap element and see whether it is expired
		 *		a) if expire == timepoint::zero(), ignore and pop, continue
		 *		b) if expire time reach, call callee, check REPEAT and push to tq if necessary, pop
		 */
		inline void update(timer_timepoint_t& nexpire) {
			{
				unique_lock<mutex> ulg(m_mutex);
				while (m_tq.empty()) {
					m_in_wait = true;
					if (!m_has_own_run_th) {
						nexpire = m_nexpire;
					} else {
						if (m_nexpire == _TIMER_TP_INFINITE) {
							m_cond.wait(ulg);
						}
						else {
							m_cond.wait_until<timer_clock_t, timer_duration_t>(ulg, m_nexpire);
						}
						m_in_wait = false;
					}
				}
				WAWO_ASSERT(m_tq.size());
				while (!m_tq.empty()) {
					timer_op& op = m_tq.front();
					WWRP<timer>& tm = op.tm;
					WAWO_ASSERT(tm->delay.count() >= 0);
					switch (op.code) {
					case OP_START:
						{
							tm->expire = timer_clock_t::now() + tm->delay;
							tm->state = S_STARTED;
							m_heap->push(std::move(tm));
						}
						break;
					case OP_STOP:
						{
							tm->expire = timer_timepoint_t();
						}
						break;
					}
					m_tq.pop();
				}
			}
			while (!m_heap->empty()) {
				WWRP<timer>& tm = m_heap->front();
				if (tm->expire == timer_timepoint_t()) {
					m_heap->pop();
					continue;
				} else if(timer_clock_t::now() < tm->expire) {
					nexpire = tm->expire;
					goto _recalc_nexpire;
				} else {
					WAWO_ASSERT(tm->state == timer_state::S_STARTED);
					tm->callee(tm, tm->cookie);
					if (tm->type == timer_type::T_REPEAT) {
						lock_guard<spin_mutex> __lg(m_mutex_tq);
						m_tq.push({ OP_START, tm });
					} else {
						tm->state = timer_state::S_EXPIRED;
					}
					m_heap->pop();
				}
			}
			nexpire = timer_timepoint_t() + timer_duration_t(~0);
		_recalc_nexpire:
			{//double check 
				lock_guard<spin_mutex> _lgtq(m_mutex_tq);
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