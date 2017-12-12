#include <wawo/thread/thread.hpp>
#include <wawo/thread/tls.hpp>
#include <wawo/log/logger_manager.h>
#include <wawo/thread/condition.hpp>

namespace wawo { namespace thread {

	thread::thread() :
		m_handler(NULL),
		m_thread_data(NULL),
		m_run_base_type(NULL)
	{
	}

	thread::~thread() {
		join();
	}

	void thread::interrupt() {
		if (m_thread_data != NULL) {
			m_thread_data->interrupt();
		}
	}

	void thread::__PRE_RUN_PROXY__() {
		WAWO_ASSERT(m_thread_data != NULL);
		tls_set<impl::thread_data>(m_thread_data);

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		MutexSet* mtxset = tls_create<MutexSet>();
		WAWO_ASSERT(mtxset != NULL);
		(void)mtxset;
#endif
		WAWO_TRACE_THREAD("[#%llu][thread]__PRE_RUN_PROXY__", wawo::this_thread::get_id());
	}

	void thread::__RUN_PROXY__() {
		__PRE_RUN_PROXY__();
		WAWO_ASSERT(m_run_base_type.get() != NULL);
		try {
			m_run_base_type->_W_run();
		} catch (impl::interrupt_exception&) {
			WAWO_WARN("[#%llu][thread]__RUN_PROXY__ thread interrupt catch", wawo::this_thread::get_id() );
		} catch (wawo::exception& e) {
			WAWO_ERR("[[#%llu]][thread]__RUN_PROXY__, wawo::exception: [%d]%s\n%s(%d) %s\n%s", wawo::this_thread::get_id(), 
				e.code, e.message, e.file, e.line, e.func, e.callstack);
			throw;
		} catch (std::exception& e) {
			WAWO_ERR("[[#%llu]][thread]__RUN_PROXY__, std::exception: %s", wawo::this_thread::get_id(), e.what());
			throw;
		} catch (...) {
			WAWO_ERR("[#%llu][thread]__RUN_PROXY__ thread unknown exception", wawo::this_thread::get_id());
			__POST_RUN_PROXY__();
			throw;
		}
		__POST_RUN_PROXY__();
	}
	void thread::__POST_RUN_PROXY__() {
#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
		tls_destroy<MutexSet>();
#endif
		tls_set<impl::thread_data>(NULL);
		WAWO_TRACE_THREAD("[#%llu][thread]__POST_RUN_PROXY__", wawo::this_thread::get_id());
	}

	namespace impl {
		void thread_data::interrupt() {
			lock_guard<spin_mutex> lg(mtx);
			interrupted = true;
			{
				if (current_cond != NULL)
				{
					lock_guard<mutex> lg_current_cond_mtx(*current_cond_mutex);
					current_cond->notify_all();
				}
				if (current_cond_any != NULL)
				{
					lock_guard<spin_mutex> lg_current_cond_mtx(*current_cond_any_mutex);
					current_cond_any->notify_all();
				}
			}
		}
	}

}}
