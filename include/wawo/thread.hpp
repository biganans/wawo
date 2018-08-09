#ifndef _WAWO_THREAD_HPP_
#define _WAWO_THREAD_HPP_

#if defined( _DEBUG) || defined(DEBUG)
	#ifndef _DEBUG_THREAD
		#define _DEBUG_THREAD
	#endif
#endif

#ifdef _DEBUG_THREAD
	#define _DEBUG_THREAD_CREATE_JOIN
#endif

#include <thread>
#include <exception>

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>

#include <wawo/exception.hpp>
#include <wawo/log/logger_manager.h>

#include <wawo/thread_impl/spin_mutex.hpp>
#include <wawo/thread_impl/mutex.hpp>
#include <wawo/thread_impl/condition.hpp>


//#define WAWO_ENABLE_TRACE_THREAD
#ifdef WAWO_ENABLE_TRACE_THREAD
#define WAWO_TRACE_THREAD WAWO_DEBUG
#else
#define WAWO_TRACE_THREAD(...)
#endif


namespace wawo {

#ifdef _DEBUG_THREAD_CREATE_JOIN
	struct thread_debugger: public wawo::singleton<thread_debugger> {
		WAWO_DECLARE_NONCOPYABLE(thread_debugger)
	public:
		std::atomic<u32_t> m_created;
		std::atomic<u32_t> m_joined;
		thread_debugger():
			m_created(0),
			m_joined(0)
		{
		}
		~thread_debugger() {}
		void init() { m_created = 0; m_joined = 0; }
		void deinit() { WAWO_CONDITION_CHECK(m_created == m_joined); }
		void increase_create() { ++m_created;}
		void increase_join() { ++m_joined;}
	};
#endif

	class thread;

	namespace impl
	{
		struct _Impl_base;
		typedef wawo::shared_ptr<_Impl_base> __shared_base_type;

		struct _Impl_base {
			virtual ~_Impl_base() {};
			virtual void _W_run() = 0;
		};

		template <typename _Callable>
		struct _Impl : public _Impl_base
		{
			_Callable _W_func;
			_Impl(_Callable&& __f) :_W_func(std::forward<_Callable>(__f))
			{}
			void _W_run() { _W_func(); }
		};

		template <typename _Callable>
		inline wawo::shared_ptr<_Impl<_Callable>> _M_make_routine(_Callable&& __f)
		{
			return wawo::make_shared<_Impl<_Callable>>(std::forward<_Callable>(__f));
		}

		struct interrupt_exception : public wawo::exception {
			interrupt_exception() :
				exception(wawo::E_THREAD_INTERRUPT, "thread interrupted", __FILE__, __LINE__,__FUNCTION__ )
			{
			}
		};

		struct thread_data {
			WAWO_DECLARE_NONCOPYABLE(thread_data)
		public:
			spin_mutex mtx;
			thread* th;
			mutex* current_cond_mutex;
			condition_variable* current_cond;

			spin_mutex* current_cond_any_mutex;
			condition_variable_any* current_cond_any;
			condition_variable_any sleep_cond_any;

			bool interrupted;
		public:
			thread_data(thread* const& thr) :
				mtx(),
				th(thr),
				current_cond_mutex(NULL),
				current_cond(NULL),
				current_cond_any(NULL),
				interrupted(false)
			{}

			~thread_data() {}

			template <class _Rep, class _Period>
			inline void sleep_for(std::chrono::duration<_Rep, _Period> const& duration) {
				wawo::unique_lock<spin_mutex> ulk(mtx);
				if (interrupted) {
					interrupted = false;
					WAWO_TRACE_THREAD("interrupt_exception triggered in sleep action");
					throw interrupt_exception();
				}
				static const std::chrono::duration<_Rep, _Period> wait_thresh = 
					std::chrono::duration_cast<std::chrono::duration<_Rep, _Period>>(std::chrono::nanoseconds(100000000ULL));
				
				if (duration <= wait_thresh) {
					std::this_thread::sleep_for(duration);
				} else {
					sleep_cond_any.wait_for(ulk, duration);
				}

				if (interrupted) {
					interrupted = false;
					WAWO_TRACE_THREAD("interrupt_exception triggered in sleep action");
					throw interrupt_exception();
				}
			}
			
			inline void interrupt();
			inline void check_interrupt() {
				wawo::lock_guard<spin_mutex> lg(mtx);
				if (interrupted) {
					interrupted = false;
					WAWO_TRACE_THREAD("interrupt_exception triggered");
					throw interrupt_exception();
				}
			}

		};
	}

	class thread final :
		public wawo::ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(thread)

		std::thread* m_handler;
		impl::thread_data* m_thread_data;
		impl::__shared_base_type m_run_base_type;

		void __PRE_RUN_PROXY__();
		void __RUN_PROXY__();
		void __POST_RUN_PROXY__();

	public:
		thread();
		~thread();

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _THREAD_CONS( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
	template<class _Fn COMMA LIST(_CLASS_TYPE)> \
		int start(_Fn _Fx COMMA LIST(_TYPE_REFREF_ARG)) \
		{ \
			WAWO_ASSERT(m_handler == NULL); \
			WAWO_ASSERT(m_thread_data == NULL); \
			m_thread_data = new impl::thread_data(this); \
			WAWO_ASSERT(m_thread_data != NULL); \
			std::thread* _THH_ = NULL; \
			try { \
				m_run_base_type = impl::_M_make_routine(std::bind(std::forward<_Fn>(_Fx) COMMA LIST(_FORWARD_ARG))); \
				_THH_ = new std::thread(&thread::__RUN_PROXY__, this); \
			} catch (...) { \
				WAWO_DELETE(m_thread_data); \
				WAWO_DELETE(_THH_); \
				int _eno = wawo::get_last_errno(); \
				WAWO_ERR("[thread]new std::thread(&thread::__RUN_PROXY__, _THH_) failed: %d", _eno); \
				return WAWO_NEGATIVE(_eno); \
			} \
			WAWO_RETURN_V_IF_MATCH(wawo::E_MEMORY_ALLOC_FAILED, (_THH_ == NULL)); \
			m_handler = _THH_; \
			return wawo::OK; \
		}

_VARIADIC_EXPAND_0X(_THREAD_CONS, , , , )
#else
		template <typename _Callable, typename... _Args>
		int start(_Callable&& __f, _Args&&... __args) {

			WAWO_ASSERT(m_handler == NULL);
			WAWO_ASSERT(m_thread_data == NULL);

			m_thread_data = new impl::thread_data(this);
			WAWO_ASSERT(m_thread_data != NULL);

			std::thread* _THH_ = NULL;
			try {
				m_run_base_type = impl::_M_make_routine(std::bind(std::forward<std::remove_reference<_Callable>::type>(__f), std::forward<_Args>(__args)...));
				_THH_ = new std::thread(&thread::__RUN_PROXY__, this);
			} catch (...) {
				WAWO_DELETE(m_thread_data);
				WAWO_DELETE(_THH_);
				int _eno = wawo::get_last_errno();
				WAWO_ERR("[thread]new std::thread(&thread::__RUN_PROXY__, _THH_) failed: %d", _eno);
				return (_eno);
			}
			WAWO_RETURN_V_IF_MATCH(wawo::E_MEMORY_ALLOC_FAILED, (_THH_ == NULL));
			m_handler = _THH_;

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread_debugger::instance()->increase_create();
#endif
			return wawo::OK;
		}
#endif

		void join() {
			if (m_handler == NULL) {
				return;
			}

			WAWO_ASSERT(m_handler->joinable());
			m_handler->join();
			WAWO_DELETE(m_handler);
			WAWO_DELETE(m_thread_data);

#ifdef _DEBUG_THREAD_CREATE_JOIN
			wawo::thread_debugger::instance()->increase_join();
#endif
		}

		void interrupt();
	};
}

#include <wawo/tls.hpp>
namespace wawo {
	namespace this_thread {

		inline void __interrupt_check_point() {
			wawo::impl::thread_data* const tdp = wawo::tls_get<wawo::impl::thread_data>();
			if (WAWO_LIKELY(tdp)) tdp->check_interrupt();
		}

		template <class _Duration>
		void sleep_for(_Duration const& dur) {
			wawo::impl::thread_data* const tdp = wawo::tls_get<wawo::impl::thread_data>();
			if (WAWO_LIKELY(tdp)) {
				tdp->sleep_for(dur);
			}
			else {
				std::this_thread::sleep_for(dur);
			}
		}

		inline void sleep(wawo::u64_t const& milliseconds) {
			wawo::this_thread::sleep_for<std::chrono::milliseconds>(std::chrono::milliseconds(milliseconds));
		}
		inline void no_interrupt_sleep(wawo::u64_t const& milliseconds) {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
		}
		inline void usleep(wawo::u64_t const& microseconds) {
			wawo::this_thread::sleep_for<std::chrono::microseconds>(std::chrono::microseconds(microseconds));
		}
		inline void no_interrupt_usleep(wawo::u64_t const& microseconds) {
			std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
		}
		inline void nsleep(wawo::u64_t const& nano) {
			wawo::this_thread::sleep_for<std::chrono::nanoseconds>(std::chrono::nanoseconds(nano));
		}
		inline void no_interrupt_nsleep(wawo::u64_t const& nano) {
			std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
		}
		inline void yield() {
			wawo::this_thread::__interrupt_check_point();
			std::this_thread::yield();
		}
		inline void no_interrupt_yield() {
			std::this_thread::yield();
		}
		inline void yield(u64_t const& k) {
			if (k < 4) {
			}
			else if (k < 28) {
				wawo::this_thread::yield();
			}
			else {
				wawo::this_thread::sleep_for<std::chrono::nanoseconds>(std::chrono::nanoseconds(k));
			}
		}
		inline void no_interrupt_yield(u64_t const& k) {
			if (k < 4) {
			}
			else if (k < 28) {
				std::this_thread::yield();
			}
			else {
				std::this_thread::sleep_for(std::chrono::nanoseconds(k));
			}
		}

		inline wawo::u64_t get_id() {
#if WAWO_ISWIN
			return static_cast<u64_t>(GetCurrentThreadId());
#else
			std::hash<std::thread::id> hasher;
			return static_cast<wawo::u64_t>(hasher(std::this_thread::get_id()));
#endif
		}
}}

#endif