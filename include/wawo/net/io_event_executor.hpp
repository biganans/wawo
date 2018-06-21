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

	namespace impl {
		class iocp;
	}


	typedef std::function<void()> fn_io_event_task;
	typedef wawo::task::task io_task;
	typedef std::queue<WWRP<wawo::task::task_abstract>> TASK_Q;

	class io_event_executor:
		public ref_base
	{
	protected:
		spin_mutex m_tq_mtx;
		condition_any m_cond;

		WWSP<TASK_Q> m_tq_standby;
		WWSP<TASK_Q> m_tq;
		WWSP<timer_manager> m_tm;
		std::thread::id m_tid;
		bool m_in_wait;

	public:
		io_event_executor():m_in_wait(false) {}
		virtual ~io_event_executor() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
		}

		inline void wait_check() {
			if (m_in_wait) m_cond.notify_one();
		}

		inline void execute(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			if (in_poller()) {
				t->run();
				return;
			}
			schedule(t);
		}

		inline void schedule(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(t);
			if (m_in_wait) m_cond.notify_one();
		}
		inline void execute(fn_io_event_task&& f) {
			if (in_poller()) {
				f();
				return;
			}
			schedule(std::forward<fn_io_event_task>(f));
		}
		inline void schedule(fn_io_event_task&& f) {
			WWRP<io_task> _t = wawo::make_ref<io_task>(std::forward<fn_io_event_task>(f));
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(_t);
			if (m_in_wait) m_cond.notify_one();
		}

		inline void wait() {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			if (m_tq_standby->size() == 0) {
				m_in_wait = true;
				m_cond.no_interrupt_wait_for<spin_mutex>(m_tq_mtx, std::chrono::microseconds(64));
				m_in_wait = false;
			}
		}

		inline bool in_poller() const {
			return std::this_thread::get_id() == m_tid;
		}

		inline void start_timer(WWRP<wawo::timer> const& t) {
			m_tm->start(t);
		}
		inline void stop_timer(WWRP<wawo::timer> const& t) {
			m_tm->stop(t);
		}

		void init() {
			m_tid = std::this_thread::get_id();
			m_tq_standby = wawo::make_shared<TASK_Q>();
			m_tq = wawo::make_shared<TASK_Q>();

			m_tm = wawo::make_shared<timer_manager>(false);
		}

		void deinit() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());

			m_tm = NULL;
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
				WWRP<wawo::task::task_abstract>& t = m_tq->front();
				WAWO_ASSERT(t != NULL);
				t->run();
				m_tq->pop();
			}
		}
	};
}}
#endif