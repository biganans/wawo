#ifndef _WAWO_NET_IO_EXECUTOR_HPP
#define _WAWO_NET_IO_EXECUTOR_HPP

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/thread/thread_run_object_abstract.hpp>
#include <wawo/thread/mutex.hpp>

#include <wawo/task/task.hpp>
#include <wawo/timer.hpp>


#include <queue>

namespace wawo { namespace net {
	//@todo impl block until no new task feature
	using namespace wawo::thread;

	typedef std::queue<WWRP<wawo::task::task_abstract>> TASK_Q;
	class io_executor :
		public wawo::ref_base,
		public wawo::net::thread_run_object_abstract
	{
		spin_mutex m_tq_mtx;
		WWSP<TASK_Q> m_tq_standby;
		WWSP<TASK_Q> m_tq;
		WWSP<timer_manager> m_tm;

		inline void __exec__() {
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

	public:
		io_executor() {}
		virtual ~io_executor() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());
		}

		inline void schedule(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			lock_guard<spin_mutex> lg(m_tq_mtx);
			m_tq_standby->push(t);
		}

		inline void start_timer(WWRP<wawo::timer> const& t) {
			m_tm->start(t);
		}
		inline void stop_timer(WWRP<wawo::timer> const& t) {
			m_tm->stop(t);
		}

		void on_start() {
			m_tq_standby = wawo::make_shared<TASK_Q>();
			m_tq = wawo::make_shared<TASK_Q>();

			m_tm = wawo::make_shared<timer_manager>(false);
		}

		void on_stop() {
			WAWO_ASSERT(m_tq->empty());
			WAWO_ASSERT(m_tq_standby->empty());

			m_tm = NULL;
		}

		void run() {
			__exec__();
		}
	};


}}
#endif