#ifndef _WAWO_NET_SOCKET_OBSERVER_HPP_
#define _WAWO_NET_SOCKET_OBSERVER_HPP_

#include <wawo/core.hpp>
#include <wawo/smart_ptr.hpp>
#include <wawo/net/observer_abstract.hpp>

#include <queue>

namespace wawo { namespace net {

	

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

		inline void watch(u8_t const& flag,int const& fd ,fn_io_event const& fn, fn_io_event_error const& err ) {
			WAWO_ASSERT(fd > 0);
			lock_guard<spin_mutex> _lg(m_ops_mutex);
			m_ops.push( {flag, OP_WATCH,fd, fn,err } );
		}

		inline void unwatch(u8_t const& flag, int const& fd) {
			WAWO_ASSERT(fd>0 );
			lock_guard<spin_mutex> _lg(m_ops_mutex);
			m_ops.push({ flag, OP_UNWATCH, fd, NULL, NULL });
		}

	private:
		WWSP<observer_abstract> m_impl;
		spin_mutex m_ops_mutex;
		event_op_queue m_ops;
		u8_t m_polltype;
	};
}} //ns of wawo::net
#endif //