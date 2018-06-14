#ifndef WAWO_THREAD_RUN_OBJECT_ABSTRACT_HPP
#define WAWO_THREAD_RUN_OBJECT_ABSTRACT_HPP

#include <wawo/core.hpp>
#include <wawo/thread.hpp>

namespace wawo {
	//will run until you call stop
	class thread_run_object_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(thread_run_object_abstract)
	private:
		enum thread_run_object_state {
			TR_IDLE,
			TR_START,
			TR_RUNNING,
			TR_STOP,
		};

	private:
		WWRP<thread> m_th;
		volatile thread_run_object_state m_state;

		int start_thread() {
			WAWO_ASSERT(m_th == NULL);
			try {
				m_th = wawo::make_ref<thread>();
			} catch (...) {
				int _eno = wawo::get_last_errno();
				WAWO_ERR("[thread_run_object_abstract::start_thread] failed: %d", _eno);
				return _eno;
			}
			m_state = TR_START;
			return m_th->start(&thread_run_object_abstract::__run__, this);
		}
		void join_thread() {
			if (m_th != NULL) {
				m_th->join();
				m_th = NULL;
			}
		}
		void interrupt_thread() {
			WAWO_ASSERT(m_th != NULL);
			m_th->interrupt();
		}

		void __on_start__() {
			WAWO_ASSERT(m_state == TR_START);
			on_start();
			m_state = TR_RUNNING; //if launch failed, we'll not reach here, and it will not in TR_RUNNING ..., so don't worry
		}
		void __on_stop__() {
			WAWO_ASSERT(m_state == TR_STOP);
			on_stop();
		}

		void operator()() {
			while (m_state == TR_RUNNING) {
				run();
			}
		}
		void __on_exit__() {}

		void __on_launching__() {
#ifdef WAWO_PLATFORM_GNU
			//check stack size
			size_t ssize = wawo::get_stack_size();
			WAWO_DEBUG("[thread_run_object_abstract::__on_launching__()]thread stack size: %d", ssize);
			WAWO_ASSERT(ssize >= (1024 * 1024 * 4)); //4M
			(void)ssize;
#endif
		}

		void __run__() {
			WAWO_ASSERT(m_th != NULL);
			try {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_launching__() begin");
				__on_launching__();
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_launching__() end");
			} catch (wawo::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__run__]__on_launching__() wawo::exception:[%d]%s\n%s(%d) %s\n%s",
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch (std::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__run__]__on_launching__() std::exception: %s", e.what());
				throw;
			} catch (...) {
				WAWO_ERR("[thread_run_object_abstract::__run__]__on_launching__() unknown thread exception");
				throw;
			}

			try {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_start__() begin");
				__on_start__();
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__run__]__on_start__() end");
			} catch (wawo::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__run__]__on_start__() wawo::exception: [%d]%s\n%s(%d) %s\n%s",
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch (std::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__run__]__on_start__() std::exception: %s", e.what());
				throw;
			} catch (...) {
				WAWO_ERR("[thread_run_object_abstract::__on_start__]__on_start__() unknown thread exception");
				throw;
			}

			try {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]thread operator() begin");
				operator()();
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]thread operator() end");
			}
			
			catch (wawo::impl::interrupt_exception&) {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::_RUN::operator()] interrupt catch" );
			} catch (wawo::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::operator()]operator() wawo::exception: [%d]%s\n%s(%d) %s\n%s",
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch (std::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::operator()]operator() std::exception: %s", e.what());
				throw;
			} catch (...) {
				WAWO_ERR("[thread_run_object_abstract::operator()]operator() unknown thread exception");
				throw;
			}

			//clear skiped interrupt execetion if it has [edge event]
			try {this_thread::__interrupt_check_point();} catch (wawo::impl::interrupt_exception&) {}

			try {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_stop__() begin");
				__on_stop__();
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_stop__() end");
			} catch (wawo::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_stop__()  wawo::exception: [%d]%s\n%s(%d) %s\n%s",
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch (std::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_stop__() std::exception : %s", e.what());
				throw;
			} catch (...) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_stop__() unknown thread exception");
				throw;
			}

			try {
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_exit__() begin");
				__on_exit__();
				WAWO_TRACE_THREAD("[thread_run_object_abstract::__RUN()__]__on_exit__() end");
			} catch (wawo::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_exit__ wawo::exception: [%d]%s\n%s(%d) %s\n%s",
					e.code, e.message, e.file, e.line, e.func, e.callstack);
				throw;
			} catch (std::exception& e) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_exit__ exception : %s", e.what());
				throw;
			} catch (...) {
				WAWO_ERR("[thread_run_object_abstract::__RUN()__]__on_exit__ unknown thread exception");
				throw;
			}
		}

	public:
		thread_run_object_abstract() :
			m_th(NULL),
			m_state(TR_IDLE)
		{
		}

		virtual ~thread_run_object_abstract() { stop(); }

		inline bool is_starting() const {
			return m_state == TR_START;
		}
		inline bool is_running() const {
			return m_state == TR_RUNNING ;
		}
		inline bool is_idle() const {
			return m_state == TR_IDLE;
		}

		virtual int start( bool const& block_start = true ) {
			WAWO_ASSERT( m_state == TR_IDLE );

			if( block_start ) {
				int start_rt = start_thread();
				if( start_rt != wawo::OK ) {
					return start_rt;
				}
				for(int k=0;is_starting();++k ) {
					wawo::this_thread::yield(k);
				}
				return wawo::OK ;
			} else {
				return start_thread() ;
			}
		}

		virtual void stop() {
			WAWO_ASSERT( ( m_state == TR_IDLE || m_state == TR_START || m_state == TR_RUNNING ) );

			if (m_state == TR_IDLE) {
				return;
			} else if (m_state == TR_START) {
				int k = 0;
				while( is_starting()) wawo::this_thread::yield(++k);
			} else if (m_state == TR_RUNNING) {
				interrupt_thread();
				m_state = TR_STOP;
			}

			join_thread();
			m_state = TR_IDLE;
		}

		virtual void on_start() = 0 ;
		virtual void on_stop() = 0 ;
		virtual void run() = 0 ;
	};
}
#endif