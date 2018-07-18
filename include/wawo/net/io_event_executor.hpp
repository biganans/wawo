#ifndef _WAWO_NET_IO_EVENT_EXECUTOR_HPP
#define _WAWO_NET_IO_EVENT_EXECUTOR_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/mutex.hpp>

#include <wawo/task/task.hpp>
#include <wawo/timer.hpp>

#include <queue>

namespace wawo { namespace net {
	//@todo impl block until no new task feature

	typedef std::function<void()> fn_io_event_task;
	typedef std::queue<fn_io_event_task> TASK_Q;

	enum wait_type {
		W_NOWAIT,
		W_COND,
		W_CUSTOM
	};

	enum wait_time {
		W_0,
		W_INFINITE,
		W_MICROSECOND
	};

	class io_event_executor:
		public ref_base
	{
	protected:
		spin_mutex m_tq_mtx;
		condition_any m_cond;

		TASK_Q* m_tq_standby_;
		TASK_Q* m_tq_standby;
		TASK_Q* m_tq;
		WWSP<timer_manager> m_tm;
		std::thread::id m_tid;
		wait_type m_wait_t;

		inline void __wait_check() {
			if (WAWO_LIKELY(m_wait_t == W_NOWAIT)) {
				return;
			} else if (m_wait_t == W_COND) {
				m_cond.notify_one();
			} else {
				interrupt_wait();
			}
		}

	public:
		io_event_executor():m_wait_t(W_NOWAIT) {}
		virtual ~io_event_executor() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
		}

		inline void execute(fn_io_event_task&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::forward<fn_io_event_task>(f));
		}
		inline void schedule(fn_io_event_task&& f) {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(f);
			__wait_check();
		}

		void cond_wait() {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			if (m_tq_standby->size() == 0) {
				m_wait_t = W_COND;
				m_cond.no_interrupt_wait_for<spin_mutex>(m_tq_mtx, std::chrono::microseconds(1024));
				m_wait_t = W_NOWAIT;
			}
		}

		//0,	NO WAIT
		//-1,	INFINITE WAIT
		//>0,	WAIT MICROSECOND
		inline int _before_wait() {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			WAWO_ASSERT(m_wait_t == W_NOWAIT);
			if (m_tq_standby->size() == 0) {
				m_wait_t = W_CUSTOM;
				return -1;
			}
			return 0;
		}
		inline void _after_wait() {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			WAWO_ASSERT(m_wait_t != W_NOWAIT);
			m_wait_t = W_NOWAIT;
		}
		virtual void interrupt_wait() { WAWO_ASSERT(!"NOT IMPLEMENTED"); }

		inline bool in_event_loop() const {
			return std::this_thread::get_id() == m_tid;
		}

		inline void start_timer(WWRP<wawo::timer> const& t) {
			m_tm->start(t);
		}
		inline void stop_timer(WWRP<wawo::timer> const& t) {
			m_tm->stop(t);
		}

		virtual void init() {
			m_tid = std::this_thread::get_id();
			m_tm = wawo::make_shared<timer_manager>(false);
			m_tq_standby = new TASK_Q();
			m_tq = new TASK_Q();
		}

		virtual void deinit() {
			//last time to exec tasks
			exec_task();
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
			m_tm = NULL;
			WAWO_DELETE(m_tq_standby);
			WAWO_DELETE(m_tq);
		}

		inline void exec_task() {
			WAWO_ASSERT(m_tq->size() == 0);
			{
				lock_guard<spin_mutex> lg(m_tq_mtx);
				if (m_tq_standby->size() > 0) {
					std::swap(m_tq, m_tq_standby);
				}
			}

			while (m_tq->size()) {
				fn_io_event_task& t = m_tq->front();
				WAWO_ASSERT(t);
				t();
				m_tq->pop();
			}
		}
	};
}}
#endif