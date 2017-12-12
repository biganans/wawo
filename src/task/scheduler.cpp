
#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/task/scheduler.hpp>

#define WAWO_MONITOR_TASK_VECTOR_CAPACITY
//#define WAWO_MONITOR_TASK_VECTOR_SIZE
#define WAWO_TASK_VECTOR_INIT_SIZE 4096
//#define WAWO_TASK_VECTOR_MONITOR_SIZE 128


namespace wawo { namespace task {

	//std::atomic<u64_t> task_abstract::s_auto_increment_id(0);

	scheduler::scheduler( u8_t const& max_runner, u8_t const& max_sequential_runner) :
		m_mutex(),
		m_condition(),
		m_state( S_IDLE ),
		m_max_concurrency(max_runner),
		m_max_seq_concurrency(max_sequential_runner),
		m_runner_pool(NULL),
		m_sequencial_runner_pool(NULL)
	{
	}

	scheduler::~scheduler() {
		stop();
		WAWO_TRACE_TASK("[scheduler]~scheduler()");
	}

	int scheduler::start() {
		{
			unique_lock<scheduler_mutext_t> _ul(m_mutex);
			if (m_state == S_EXIT) {
				m_state = S_IDLE;
			}
		}
		__on_start();
		return wawo::OK;
	}

	void scheduler::stop() {
		{
			WAWO_TRACE_TASK("scheduler::stop begin");
			{
				lock_guard<scheduler_mutext_t> _ul(m_mutex);
				if (m_state == S_EXIT || m_state == S_IDLE) { return; }
			}
			WAWO_TRACE_TASK("scheduler::stop __block_until_no_new_task()");

			//experiment
			__block_until_no_new_task();
			WAWO_TRACE_TASK("scheduler::stop __block_until_no_new_task() exit");

			lock_guard<scheduler_mutext_t> _ul( m_mutex );
			if( m_state == S_EXIT || m_state == S_IDLE ) { return; }

			m_state = S_EXIT;
			m_condition.notify_one();
		}
		WAWO_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop");
		__on_stop();
		WAWO_TRACE_TASK("scheduler::stop __block_until_no_new_task() on_stop exit");
	}

	void scheduler::__on_start() {
		unique_lock<scheduler_mutext_t> _ul( m_mutex );

		m_state = S_RUN;
		m_tasks_assigning = new priority_task_queue();
		m_tasks_runner_wait_count = 0;

		m_runner_pool = new runner_pool( m_max_concurrency ) ;
		WAWO_ALLOC_CHECK( m_runner_pool, sizeof(runner_pool) ) ;
		m_runner_pool->init(this);

		WAWO_ASSERT(m_max_seq_concurrency>0&& m_max_seq_concurrency<128);
		m_sequencial_runner_pool = new sequencial_runner_pool(m_max_seq_concurrency);
		WAWO_ALLOC_CHECK( m_sequencial_runner_pool, sizeof(sequencial_runner_pool) ) ;
		m_sequencial_runner_pool->init();
	}

	void scheduler::__on_stop() {
		WAWO_ASSERT( m_state == S_EXIT );

		WAWO_ASSERT( m_tasks_assigning->empty() );

		m_runner_pool->deinit();
		m_sequencial_runner_pool->deinit();

		WAWO_DELETE( m_tasks_assigning );

		WAWO_DELETE( m_runner_pool );
		WAWO_DELETE( m_sequencial_runner_pool );
	}
}}//end of ns
