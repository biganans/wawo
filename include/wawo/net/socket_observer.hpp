#ifndef _WAWO_NET_SOCKET_OBSERVER_HPP_
#define _WAWO_NET_SOCKET_OBSERVER_HPP_

#include <queue>

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/thread/ticker.hpp>

#include <wawo/net/observer_abstract.hpp>


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

	typedef std::queue<WWRP<wawo::task::task_abstract>> TASK_Q;

	class observer :
		public wawo::ref_base,
		public wawo::net::thread_run_object_abstract
	{
		WWRP<socket_observer> m_default;
		WWSP<wawo::thread::fn_ticker> m_ticker_default;

#ifdef WAWO_ENABLE_WCP
		WWRP<socket_observer> m_wcp;
#endif
		spin_mutex m_tq_mtx;
		WWSP<TASK_Q> m_tq_standby;
		WWSP<TASK_Q> m_tq;

	public:
		observer()
		{}
		~observer()
		{
		}

		void on_start() {

			m_tq_standby = wawo::make_shared<TASK_Q>();
			m_tq = wawo::make_shared<TASK_Q>();

			WAWO_ASSERT(m_default == NULL);
			m_default = wawo::make_ref<socket_observer>(get_os_default_poll_type());
			WAWO_ALLOC_CHECK(m_default, sizeof(socket_observer) );
			m_default->init();

#ifdef WAWO_ENABLE_WCP
			WAWO_ASSERT(m_wcp == NULL);
			m_wcp = wawo::make_ref<socket_observer>(T_WPOLL);
			WAWO_ALLOC_CHECK(m_wcp, sizeof(socket_observer) );
			m_wcp->init();
#endif

		}

		void on_stop() {
			WAWO_ASSERT(m_default != NULL);

			m_default->deinit();
			m_default = NULL;

#ifdef WAWO_ENABLE_WCP
			WAWO_ASSERT(m_wcp != NULL);
			m_wcp->deinit();
			//m_wcp = NULL;
#endif
		}

		void run() {
			_exec_tasks();
			m_default->update();
			_exec_tasks();
		}

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

		void watch(u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err, bool const& is_wcp = false );
		void unwatch(u8_t const& flag, int const& fd, bool const& is_wcp = false );

#ifdef WAWO_ENABLE_WCP
		/*
		void wcp_watch(u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err);
		void wcp_unwatch(u8_t const& flag, int const& fd);
		*/
#endif
	};

	typedef std::vector<WWRP<observer>> observer_vectors;
	class observers : public wawo::singleton<observers>
	{

	private:
		std::atomic<int> m_curr;
		observer_vectors m_observers;

	public:
		observers()
			:m_curr(0)
		{}

		void init() {
			int i = std::thread::hardware_concurrency();
			while (i-- > 0) {
				WWRP<observer> o = wawo::make_ref<observer>();
				int rt = o->start();
				WAWO_ASSERT(rt == wawo::OK);
				m_observers.push_back(o);
			}
		}

		WWRP<observer> next() {
			int i = m_curr.load() % m_observers.size() ;
			wawo::atomic_increment(&m_curr);
			return m_observers[i% m_observers.size()];
		}

		void deinit() {
			std::for_each(m_observers.begin(), m_observers.end(), [](WWRP<observer> const& o) {
				o->stop();
			});
			m_observers.clear();
		}
	};
}} //ns of wawo::net
#endif //