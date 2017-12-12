#ifndef _WAWO_NET_IOEVENTTASK_HPP
#define _WAWO_NET_IOEVENTTASK_HPP

#include <wawo/task/scheduler.hpp>

namespace wawo { namespace net {

	enum ioe_task_type {
		IOET_DEFAULT,
		IOET_WCP
	};

	
	struct observer_ctx;


	
	/*
	class wcp_io_event_task :
		public wawo::task::task_abstract
	{
		WAWO_DECLARE_NONCOPYABLE(wcp_io_event_task)

	public:
		WWRP<socket_observer> observer;
		WWRP<socket> so;
		u8_t id;

		explicit wcp_io_event_task(WWRP<socket_observer> const& observer, WWRP<socket> const& so, u8_t const& id ) :
			task_abstract(),
			observer(observer),
			so(so),
			id(id)
		{
		}

//		bool isset() {
//			return (so != NULL) && (observer != NULL);
//		}

//		void reset() {
//			so = NULL;
//			observer = NULL;
//		}
//
		void run();
	};
*/
}}

#endif