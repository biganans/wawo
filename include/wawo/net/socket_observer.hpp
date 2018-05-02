#ifndef _WAWO_NET_SOCKET_OBSERVER_HPP_
#define _WAWO_NET_SOCKET_OBSERVER_HPP_

#include <queue>

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>

#include <wawo/net/observer_abstract.hpp>
#include <wawo/net/io_executor.hpp>

namespace wawo { namespace net {

	using namespace wawo::thread;

#define WAWO_HIGH_IO_PERFORMANCE

#if WAWO_ISWIN
#ifdef _DEBUG
	const static u32_t observer_checker_interval = 256 * 1000;
#else
	const static u32_t observer_checker_interval = 8 * 1000;
#endif
#else
#ifdef WAWO_HIGH_IO_PERFORMANCE
	const static u32_t observer_checker_interval = 32 * 1000;
#else
	const static u32_t observer_checker_interval = 256 * 1000;
#endif
#endif

	class socket_observer
		:public ref_base
	{
		enum event_op_code {
			OP_WATCH,
			OP_UNWATCH
		};

		struct event_op {
			u8_t flag;
			u8_t op;
			int fd;
			WWRP<ref_base> cookie;
			fn_io_event fn;
			fn_io_event_error err;
		};

		typedef std::queue<event_op> event_op_queue ;

		enum socket_observer_state {
			S_IDLE = 0,
			S_RUN,
			S_EXIT
		};

		void _process_ops() ;
		void _alloc_impl();
		void _dealloc_impl();

	public:
		socket_observer(u8_t const& type) ;
		~socket_observer() ;

		void init() ;
		void deinit() ;
		void update();

		inline void watch(u8_t const& flag,int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err ) {
			WAWO_ASSERT(fd > 0);
			lock_guard<spin_mutex> _lg(m_ops_mutex);
			m_ops.push( {flag, OP_WATCH,fd, cookie,fn,err } );
		}

		inline void unwatch(u8_t const& flag, int const& fd) {
			WAWO_ASSERT(fd>0 );
			lock_guard<spin_mutex> _lg(m_ops_mutex);
			m_ops.push({ flag, OP_UNWATCH, fd, NULL, NULL, NULL });
		}

	private:
		WWSP<observer_abstract> m_impl;
		spin_mutex m_ops_mutex;
		event_op_queue m_ops;
		u8_t m_polltype;
	};

	class observer:
		public io_executor
	{
		WWRP<socket_observer> m_observer;
		//spin_mutex m_tq_mtx;
		//WWSP<TASK_Q> m_tq_standby;
		//WWSP<TASK_Q> m_tq;

		u8_t m_observer_type;
	public:
		observer( u8_t t = get_os_default_poll_type()):
			m_observer_type(t)
		{}
		~observer(){}

		u8_t get_type() const { return m_observer_type; }

		void on_start() {
			//m_tq_standby = wawo::make_shared<TASK_Q>();
			//m_tq = wawo::make_shared<TASK_Q>();
			
			io_executor::on_start();

			WAWO_ASSERT(m_observer == NULL);
			m_observer = wawo::make_ref<socket_observer>(m_observer_type);
			WAWO_ALLOC_CHECK(m_observer, sizeof(socket_observer) );
			m_observer->init();
		}

		void on_stop() {
			io_executor::on_stop();

			WAWO_ASSERT(m_observer != NULL);
			m_observer->deinit();
			m_observer = NULL;
		}

		void run() {
			io_executor::run();
			m_observer->update();
			io_executor::run();
			wawo::this_thread::nsleep(observer_checker_interval);
		}
		/*
		void _plan_task(WWRP<wawo::task::task_abstract> const& t) {
			WAWO_ASSERT(t != NULL);
			m_tq_standby->push(t);
		}

		void _exec_tasks() {
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

		void plan(WWRP<wawo::task::task_abstract> const& t) {
			lock_guard<spin_mutex> lg(m_tq_mtx);
			_plan_task(t);
		}
		*/

		void watch(u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err );
		void unwatch(u8_t const& flag, int const& fd );
	};

	typedef std::vector<WWRP<observer>> observer_vectors;
	class observers : public wawo::singleton<observers>
	{

	private:
		std::atomic<int> m_curr_sys;
		std::atomic<int> m_curr_wpoll;
		observer_vectors m_observers;
		observer_vectors m_wpolls;
	public:
		observers()
			:m_curr_sys(0), m_curr_wpoll(0)
		{}
		~observers() {}
		void init(int wpoller_count = 1);
		WWRP<observer> next(bool const& return_wpoller = false);
		void deinit();
	};
}} //ns of wawo::net
#endif //
