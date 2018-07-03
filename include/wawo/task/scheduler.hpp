#ifndef _WAWO_TASK_TASK_DISPATCHER_HPP_
#define _WAWO_TASK_TASK_DISPATCHER_HPP_

#include <vector>

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/singleton.hpp>

#include <wawo/thread.hpp>
#include <wawo/task/task.hpp>
#include <wawo/task/runner.hpp>

//#ifdef WAWO_PLATFORM_WIN
//	#define WAWO_SCHEDULER_USE_SPIN
//#endif
//#define WAWO_ENABLE_SEQUENCIAL_RUNNER


namespace wawo { namespace task {

#ifdef WAWO_SCHEDULER_USE_SPIN
	typedef spin_mutex scheduler_mutext_t;
#else
	typedef mutex scheduler_mutext_t;
#endif

	class scheduler:
		public wawo::singleton<scheduler>
	{
		friend class runner;
		WAWO_DECLARE_NONCOPYABLE(scheduler)

	private:
		scheduler_mutext_t m_mutex;

#ifdef WAWO_SCHEDULER_USE_SPIN
		condition_any m_condition;
#else
		condition m_condition;
#endif
		volatile int m_state;
		u8_t m_tasks_runner_wait_count;
		u8_t m_max_concurrency;
		priority_task_queue* m_tasks_assigning;
		runner_pool* m_runner_pool;

#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
		u8_t m_max_seq_concurrency;
		sequencial_runner_pool* m_sequencial_runner_pool;
#endif

	public:
		enum task_manager_state {
			S_IDLE,
			S_RUN,
			S_EXIT,
		};


		scheduler(u8_t const& max_runner_count = static_cast<u8_t>(std::thread::hardware_concurrency())
#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
			,u8_t const& max_sequential_runner = static_cast<u8_t>(WAWO_MAX2(1, static_cast<int>(std::thread::hardware_concurrency() >> 2)))
#endif
		);

		~scheduler();

		inline void schedule( WWRP<task_abstract> const& ta, u8_t const& p = P_NORMAL ) {
			lock_guard<scheduler_mutext_t> _lg(m_mutex);
			WAWO_ASSERT( m_state == S_RUN );
			m_tasks_assigning->push(ta, p);
			if (m_tasks_runner_wait_count > 0) m_condition.notify_one();
		}

		inline void schedule(fn_task_void const& task_fn_, u8_t const& priority = wawo::task::P_NORMAL) {
			schedule(wawo::make_ref<task>(task_fn_), priority);
		}

#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
		inline void schedule(WWRP<sequencial_task> const& ta) {
			WAWO_ASSERT(m_state == S_RUN);
			m_sequencial_runner_pool->assign_task(ta);
		}
#endif

		void set_concurrency( u8_t const& max ) {
			unique_lock<scheduler_mutext_t> _lg( m_mutex );

			WAWO_ASSERT( m_state != S_RUN );
			m_max_concurrency = max;
		}

#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
		void set_seq_concurrency(u8_t const& max) {
			unique_lock<scheduler_mutext_t> _lg(m_mutex);

			WAWO_ASSERT(m_state != S_RUN);
			m_max_seq_concurrency = max;
		}
#endif
		inline u8_t const& get_max_task_runner() const {return m_runner_pool->get_max_task_runner();}

		int start();
		void stop();

		void __on_start();
		void __on_stop();


		void __block_until_no_new_task() {
		
			u8_t self_flag = 0;
			u8_t runner_flag = 0;
			while (1) {
			begin:

				if (self_flag == 0) {
					while (!m_tasks_assigning->empty()) {
						wawo::this_thread::no_interrupt_yield(50);
						runner_flag = 0;
					}
					self_flag = 1;
				}
				else if (self_flag == 1) {
					while (!m_tasks_assigning->empty()) {
						self_flag = 0;
						runner_flag = 0;
						wawo::this_thread::no_interrupt_yield(50);
						goto begin;
					}
					self_flag = 2;
				}

				if (runner_flag == 0) {
					while (!m_runner_pool->test_waiting_step1() 
#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
						|| !m_sequencial_runner_pool->test_waiting_step1()
#endif						
						) {
						wawo::this_thread::no_interrupt_yield(50);
						self_flag = 0;
						runner_flag = 0;
						goto begin;
					}
					runner_flag = 1;
				}
				else if (runner_flag == 1) {
					while (!m_runner_pool->test_waiting_step2() 
#ifdef WAWO_ENABLE_SEQUENCIAL_RUNNER
						|| !m_sequencial_runner_pool->test_waiting_step2()
#endif
						) {
						wawo::this_thread::no_interrupt_yield(50);
						self_flag = 0;
						runner_flag = 0;
						goto begin;
					}
					runner_flag = 2;
				}

				if (self_flag == 2 && runner_flag == 2) {
					break;
				}
			}
		}
	};
}}

#define WAWO_SCHEDULER (wawo::task::scheduler::instance())
#endif