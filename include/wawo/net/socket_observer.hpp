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

	class observer_ticker :
		public ticker<observer_checker_interval, 8>,
		public wawo::singleton<observer_ticker>
	{
	};

	class socket_observer
		:virtual public ref_base
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
		observer_abstract* m_impl;
		spin_mutex m_ops_mutex;
		event_op_queue m_ops;
		WWSP<wawo::thread::fn_ticker> m_ticker;
		u8_t m_polltype;
	};

	class observer :
		public wawo::singleton<observer>
	{
		WWRP<socket_observer> m_default;

#ifdef WAWO_ENABLE_WCP
		WWRP<socket_observer> m_wcp;
#endif

	public:
		observer()
		{}
		~observer() 
		{
			//WAWO_ASSERT(m_default == NULL);
			//WAWO_ASSERT(m_wcp == NULL);
		}

		void start() {
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

		void stop() {
			WAWO_ASSERT(m_default != NULL);
			m_default->deinit();
			//m_default = NULL;

#ifdef WAWO_ENABLE_WCP
			WAWO_ASSERT(m_wcp != NULL);
			m_wcp->deinit();
			//m_wcp = NULL;
#endif
		}

		void watch(u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err);
		void unwatch(u8_t const& flag, int const& fd );

#ifdef WAWO_ENABLE_WCP
		void wcp_watch(u8_t const& flag, int const& fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err);
		void wcp_unwatch(u8_t const& flag, int const& fd);
#endif
	};

	inline static void watch(bool const& iswcp, u8_t const& flag, int const&fd, WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err) {
		WAWO_ASSERT(flag > 0);
		WAWO_ASSERT(fd > 0);
		WAWO_ASSERT(cookie != NULL);
		WAWO_ASSERT(fn != NULL);
		WAWO_ASSERT(err != NULL);

#ifdef WAWO_ENABLE_WCP
		if (WAWO_UNLIKELY(iswcp)) {
			observer::instance()->wcp_watch(flag, fd, cookie, fn, err);
		}
		else {
#endif
			observer::instance()->watch(flag, fd, cookie, fn, err);
#ifdef WAWO_ENABLE_WCP
		}
#endif
	}

	inline static void unwatch(bool const& iswcp, u8_t const& flag, int const& fd) {

		WAWO_ASSERT(flag > 0);
		WAWO_ASSERT(fd > 0);

#ifdef WAWO_ENABLE_WCP
		if (WAWO_UNLIKELY(iswcp)) {
			observer::instance()->wcp_unwatch(flag, fd);
		}
		else {
#endif
			observer::instance()->unwatch(flag, fd);
#ifdef WAWO_ENABLE_WCP
		}
#endif
	}

}} //ns of wawo::net
#endif //