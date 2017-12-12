#include <wawo/log/logger_manager.h>
#include <wawo/task/scheduler.hpp>
#include <wawo/task/runner.hpp>
#include <wawo/task/scheduler.hpp>

namespace wawo { namespace task {
	using namespace wawo::thread;

	runner::runner( u8_t const& id, scheduler* s ) :
		m_id( id ),
		m_wait_flag(0),
		m_state(TR_S_IDLE),
		m_scheduler(s)
	{
		WAWO_TRACE_TASK( "[TRunner][-%u-]construct new runner", m_id );
	}

	runner::~runner() {
		stop() ;
		WAWO_TRACE_TASK( "[TRunner][-%u-]destructTaskRunner", m_id );
	}

	void runner::on_start() {
		m_state = TR_S_IDLE ;
	}

	void runner::on_stop() {
		WAWO_TRACE_TASK("[TRunner][-%u-]runner exit...", m_id ) ;
	}

	void runner::stop() {
		{
			while (m_state != TR_S_WAITING) {
				wawo::this_thread::no_interrupt_yield(1);
			}

			m_state = TR_S_ENDING;
			WAWO_TRACE_TASK("[TRunner][-%u-]runner enter into state [%d]", m_id, m_state ) ;
		}

		{
			lock_guard<scheduler_mutext_t> lg_schd(m_scheduler->m_mutex);
			m_scheduler->m_condition.no_interrupt_notify_all();
		}
		thread_run_object_abstract::stop();
	}

	void runner::run() {

		WAWO_ASSERT(m_scheduler != NULL);
		while (1)
			{
				WWRP<wawo::task::task_abstract> task;

				{
				unique_lock<scheduler_mutext_t> _ulk(m_scheduler->m_mutex);

				check_begin:
					{
						//lock_guard<spin_mutex> _lg(m_mutex);
						if (m_state == TR_S_ENDING) {
							break;
						}
					}

					m_scheduler->m_tasks_assigning->front_and_pop(task);
					if (task == NULL) {
#ifdef _DEBUG
						WAWO_ASSERT(m_scheduler->m_tasks_assigning->empty());
#endif
						m_state = TR_S_WAITING;
						++(m_scheduler->m_tasks_runner_wait_count);
						m_scheduler->m_condition.no_interrupt_wait(_ulk);
						--(m_scheduler->m_tasks_runner_wait_count);
						goto check_begin;
					}
				}
			WAWO_ASSERT(task != NULL);

			m_state = TR_S_RUNNING;
			m_wait_flag = 0;
			try {
				WAWO_TRACE_TASK("[TRunner][-%d-]task run begin, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
				task->run();
				WAWO_TRACE_TASK("[TRunner][-%d-]task run end, TID: %llu", m_id, reinterpret_cast<u64_t>(task.get()));
			}
			catch (wawo::exception& e) {
				WAWO_ERR("[TRunner][-%d-]runner wawo::exception: [%d]%s\n%s(%d) %s\n%s",m_id,
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			}
			catch (std::exception& e) {
				WAWO_ERR("[TRunner][-%d-]runner exception: %s", m_id, e.what());
				throw;
			}
			catch (...) {
				WAWO_ERR("[TRunner][-%d-]runner, unknown exception", m_id);
				throw;
			}
		}
	}


#define WAWO_SQT_VECTOR_SIZE 4000

	sequencial_runner::sequencial_runner(u8_t const& id):
		m_mutex(),
		m_condition(),
		m_wait_flag(0),
		m_state(S_IDLE),
		m_id(id),
		m_standby(NULL),
		m_assigning(NULL)
	{
	}

	sequencial_runner::~sequencial_runner() {
		stop();
	}

	void sequencial_runner::on_start() {
			lock_guard<task_runner_mutex_t> lg(m_mutex);

			m_state = S_RUN;

			m_standby = new sequencial_task_vector() ;
			WAWO_ALLOC_CHECK( m_standby, sizeof(sequencial_task_vector) );

			m_assigning = new sequencial_task_vector();
			WAWO_ALLOC_CHECK( m_assigning, sizeof(sequencial_task_vector) ) ;

			m_standby->reserve( WAWO_SQT_VECTOR_SIZE );
			m_assigning->reserve( WAWO_SQT_VECTOR_SIZE );
	}

	void sequencial_runner::stop() {
		{
			while (m_standby->size() || m_assigning->size() ) {
				wawo::this_thread::yield(1);
			}

			lock_guard<task_runner_mutex_t> lg(m_mutex);

			if( m_state == S_EXIT ) return ;
			m_state = S_EXIT ;
			m_condition.no_interrupt_notify_one();
		}

		thread_run_object_abstract::stop();
	}

	void sequencial_runner::on_stop() {
		WAWO_ASSERT( m_state == S_EXIT );
		WAWO_ASSERT( m_standby->empty() );
		WAWO_ASSERT( m_assigning->empty() );
		WAWO_DELETE( m_standby );
		WAWO_DELETE( m_assigning );
	}

	void sequencial_runner::run() {
		{
			WAWO_ASSERT(m_assigning->empty());
			unique_lock<task_runner_mutex_t> _ulk( m_mutex );

			if( m_state == S_EXIT ) {
				return ;
			}

			while( m_standby->empty() ) {
				m_state = S_WAITING;
				m_condition.no_interrupt_wait( _ulk );

				if( m_state == S_EXIT ) {
					return ;
				}
				WAWO_ASSERT( m_state == S_WAITING);
			}

			WAWO_ASSERT( m_assigning->empty() );
			WAWO_ASSERT( !m_standby->empty());
			wawo::swap( m_standby, m_assigning );
			WAWO_ASSERT( !m_assigning->empty() );
			WAWO_ASSERT( m_standby->empty() );
			m_state = S_RUN;
			m_wait_flag = 0;
		}

		int i = 0;
		int size = m_assigning->size();
		while( m_state == S_RUN && i != size ) {
			WWRP<sequencial_task>& task = (*m_assigning)[i++] ;
			WAWO_ASSERT( task != NULL );
			try {
				task->run();
			} catch ( wawo::exception& e ) {
				WAWO_ERR("[SQTRunner][-%d-]wawo::exception: [%d]%s\n%s(%d) %s\n%s", m_id, 
					e.code,e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch( std::exception& e) {
				WAWO_ERR("[SQTRunner][-%d-]std::exception: %s", m_id, e.what());
				throw;
			} catch( ... ) {
				WAWO_ERR("[SQTRunner][-%d-]unknown exception: %s", m_id );
				throw;
			}
		}
		WAWO_ASSERT( i == size );

		m_assigning->clear();
		if( (m_state == S_RUN) && (size>WAWO_SQT_VECTOR_SIZE) ) {
			m_assigning->reserve(WAWO_SQT_VECTOR_SIZE);
		}
	}


	runner_pool::runner_pool(u8_t const& max_runner) :
		m_mutex(),
		m_runners(),
		m_is_running(false),
		m_max_concurrency(max_runner),
		m_last_runner_idx(0)

	{
		WAWO_ASSERT(max_runner <= WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT);
	}

	runner_pool::~runner_pool() {}

	void runner_pool::init( scheduler* s ) {
		wawo::thread::lock_guard<mutex> _lg(m_mutex);
		m_is_running = true;
		m_last_runner_idx = 0;

		u8_t i = 0;
		while (m_runners.size() < m_max_concurrency) {
			WWRP<runner> tr = wawo::make_ref<runner>(i,s);
			WAWO_ALLOC_CHECK(tr, sizeof(runner));
			int rt = tr->start();
			if (rt != wawo::OK) {
				tr->stop();
				WAWO_THROW("create thread failed");
			}
			++i;
			m_runners.push_back(tr);
		}
	}

	void runner_pool::deinit() {
		wawo::thread::lock_guard<mutex> _lg(m_mutex);
		if (m_is_running == false) { return; }

		m_is_running = false;
		while (!m_runners.empty()) {
			m_runners.pop_back();
		}
	}

	void runner_pool::set_max_task_runner(u8_t const& count) {
		unique_lock<mutex> _lg(m_mutex);
		WAWO_ASSERT(count>0 && count<WAWO_MAX_TASK_RUNNER_CONCURRENCY_COUNT);

		m_max_concurrency = count;

		//dynamic clear ,,if ...
		while (m_runners.size() > m_max_concurrency) {
			m_runners.pop_back();
		}
	}

	//for sequence task
	sequencial_runner_pool::sequencial_runner_pool(u8_t const& concurrency) :
		m_mutex(),
		m_is_running(false),
		m_concurrency(concurrency),
		m_runners()
	{
	}

	sequencial_runner_pool::~sequencial_runner_pool() {
		deinit();
	}

	void sequencial_runner_pool::init() {
		m_is_running = true;
		WAWO_ASSERT(m_runners.size() == 0);
		u8_t i = 0;
		while (i++ < m_concurrency) {
			WWRP<sequencial_runner> r = wawo::make_ref<sequencial_runner>(i);
			WAWO_ALLOC_CHECK(r, sizeof(sequencial_runner));
			int launch_rt = r->start();
			WAWO_CONDITION_CHECK(launch_rt == wawo::OK);
			m_runners.push_back(r);
		}
		WAWO_ASSERT(m_runners.size() == m_concurrency);
	}

	void sequencial_runner_pool::deinit() {
		m_is_running = false;
		while (m_runners.size()) {
			m_runners.pop_back();
		}
		WAWO_ASSERT(m_runners.size() == 0);
	}

}}//END OF NS
