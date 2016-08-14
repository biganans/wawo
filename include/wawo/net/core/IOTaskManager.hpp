#ifndef _WAWO_NET_CORE_IO_HPP_
#define _WAWO_NET_CORE_IO_HPP_

#include <thread>
#include <wawo/Singleton.hpp>
#include <wawo/task/TaskManager.hpp>

namespace wawo { namespace net { namespace core {

class IOTaskManager:
	public wawo::Singleton<IOTaskManager>,
	public wawo::task::TaskManager
{
	public:
		IOTaskManager():
#ifdef WAWO_PLATFORM_WIN
			TaskManager( std::thread::hardware_concurrency()*2, WAWO_MAX2(1,std::thread::hardware_concurrency()>>1) )
#elif defined(WAWO_PLATFORM_GNU)
			TaskManager(std::thread::hardware_concurrency() *4, WAWO_MAX2(1, std::thread::hardware_concurrency()>>1) )
#else
	#error
#endif
		{
		}
	};
}}}

#define WAWO_NET_CORE_IO_TASK_MANAGER (wawo::net::core::IOTaskManager::GetInstance())
#endif