#ifndef _WAWO_NET_IO_EVENT_EXECUTOR_HPP
#define _WAWO_NET_IO_EVENT_EXECUTOR_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/mutex.hpp>

#include <wawo/task/task.hpp>
#include <wawo/timer.hpp>

#include <queue>

namespace wawo { namespace net {
	typedef std::function<void()> fn_io_event_task;
	typedef std::queue<fn_io_event_task> TASK_Q;

	class io_event_executor:
		public ref_base
	{
	protected:
		spin_mutex m_mutex;
		TASK_Q* m_tq_standby;
		TASK_Q* m_tq;
		WWSP<timer_manager> m_tm;
		std::thread::id m_tid;
		timer_timepoint_t m_wait_until;

		__WW_FORCE_INLINE void __wait_check( timer_timepoint_t const& nexpire ) {
			if ((m_wait_until == _TIMER_TP_INFINITE) ||
				(nexpire < m_wait_until))
			{
				interrupt_wait();
			}
		}

	public:
		io_event_executor() {}
		virtual ~io_event_executor() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
		}

		__WW_FORCE_INLINE void execute(fn_io_event_task&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::forward<fn_io_event_task>(f));
		}
		__WW_FORCE_INLINE void schedule(fn_io_event_task&& f) {
			lock_guard<spin_mutex> lg(m_mutex);
			m_tq_standby->push(f);
			__wait_check(timer_clock_t::now());
		}
		__WW_FORCE_INLINE void start_timer(WWRP<wawo::timer>&& t) {
			if (in_event_loop()) {
				m_tm->start(std::forward<WWRP<wawo::timer>>(t));
				return;
			}
			lock_guard<spin_mutex> lg(m_mutex);
			const timer_timepoint_t expire = m_tm->start(std::forward<WWRP<wawo::timer>>(t));
			__wait_check(expire);
		}
		__WW_FORCE_INLINE void start_timer(WWRP<wawo::timer> const& t) {
			if (in_event_loop()) {
				m_tm->start(t);
				return;
			}
			lock_guard<spin_mutex> lg(m_mutex);
			const timer_timepoint_t expire = m_tm->start(t);
			__wait_check(expire);
		}

		//0,	NO WAIT
		//~0,	INFINITE WAIT
		//>0,	WAIT nanosecond
		__WW_FORCE_INLINE long long _calc_wait_dur_in_nano() {
			lock_guard<spin_mutex> lg(m_mutex);
			WAWO_ASSERT(m_tm != NULL);
			wawo::timer_duration_t ndelay;
			m_tm->update(ndelay);

			if (ndelay == wawo::timer_duration_t() ||
				m_tq_standby->size() !=0
			) {
				m_wait_until = timer_timepoint_t();
				return 0;
			} else if (ndelay == _TIMER_DURATION_INFINITE){
				m_wait_until = _TIMER_TP_INFINITE;
			} else {
				m_wait_until = timer_clock_t::now() + ndelay;
			}
			return ndelay.count();
		}

		virtual void interrupt_wait() { WAWO_ASSERT(!"NOT IMPLEMENTED"); }

		__WW_FORCE_INLINE bool in_event_loop() const {
			return std::this_thread::get_id() == m_tid;
		}

		virtual void init() {
			m_tid = std::this_thread::get_id();
			m_tm = wawo::make_shared<timer_manager>(false);
			m_tq_standby = new TASK_Q();
			m_tq = new TASK_Q();
		}

		virtual void deinit() {
			//last time to exec tasks

		task_check:
			exec_task();
			if (m_tq_standby->size()) {
				goto task_check;
			}

			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
			m_tm = NULL;
			WAWO_DELETE(m_tq_standby);
			WAWO_DELETE(m_tq);
		}

		__WW_FORCE_INLINE void exec_task() {
			WAWO_ASSERT(m_tq->size() == 0);
			{
				lock_guard<spin_mutex> lg(m_mutex);
				if (m_tq_standby->size() > 0) {
					std::swap(m_tq, m_tq_standby);
				}
			}

			while (m_tq->size()) {
				fn_io_event_task const& t = m_tq->front();
				WAWO_ASSERT(t);
				t();
				m_tq->pop();
			}
		}
	};
}}
#endif