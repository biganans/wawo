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
	typedef wawo::task::task io_task;
	typedef std::queue<WWRP<wawo::task::task_abstract>> TASK_Q;

	class io_event_executor
	{
		spin_mutex m_tq_mtx;
		WWSP<TASK_Q> m_tq_standby;
		WWSP<TASK_Q> m_tq;
		WWSP<timer_manager> m_tm;
		std::thread::id m_tid;

	public:
		io_event_executor() {}
		virtual ~io_event_executor() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
		}

		inline void execute(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			if (in_event_loop()) {
				t->run();
				return;
			}
			schedule(t);
		}
		inline void schedule(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(t);
		}
		inline void execute(fn_io_event_task&& f) {
			if (in_event_loop()) {
				f();
				return;
			}
			schedule(std::forward<fn_io_event_task>(f));
		}
		inline void schedule(fn_io_event_task&& f) {
			WWRP<io_task> _t = wawo::make_ref<io_task>(std::forward<fn_io_event_task>(f));
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(_t);
		}

		//inline u32_t task_count() const {
		//	return 
		//}

		inline bool in_event_loop() const {
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