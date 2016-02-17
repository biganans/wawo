#ifndef _WAWO_NET_CORE_IO_HPP_
#define _WAWO_NET_CORE_IO_HPP_

#include <thread>
#include <wawo/SafeSingleton.hpp>
#include <wawo/task/TaskManager.h>

namespace wawo { namespace net { namespace core {

class IOTaskManager:
	public wawo::SafeSingleton<IOTaskManager>,
	public wawo::task::TaskManager
{
	public:
		IOTaskManager():
#ifdef WAWO_PLATFORM_WIN
			TaskManager( std::thread::hardware_concurrency()*2 )
#elif defined(WAWO_PLATFORM_POSIX)
			TaskManager( std::thread::hardware_concurrency() )
#else
	#error
#endif
		{
		}
	};
}}}

#define WAWO_IO_TASK_MANAGER (wawo::net::core::IOTaskManager::GetInstance())
#endif