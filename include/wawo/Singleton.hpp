#ifndef _WAWO_SINGLETON_HPP_
#define _WAWO_SINGLETON_HPP_

#include <mutex>
#include <atomic>
#include <wawo/core.h>

namespace wawo {

	template<class T>
	class Singleton {

	public:
		inline static T* GetInstance() {

			T* ins = s_instance.load(std::memory_order_acquire);
			if ( NULL == ins) {
				static std::mutex __s_instance_mutex;
				__s_instance_mutex.lock();
				ins = s_instance.load(std::memory_order_relaxed);
				if( NULL == ins ) {
					try {
						ins = new T();
						ScheduleForDestroy( Singleton<T>::DestroyInstance );
						s_instance.store(ins, std::memory_order_release);
					} catch( ... ) {
						//issue might be
						delete ins;
						WAWO_THROW_EXCEPTION( "new T() | ScheduleForDestroy failed!!!" );
					}
				}
				__s_instance_mutex.unlock();
			}

			return ins;
		}

		static void DestroyInstance() {
			T* ins = s_instance.load(std::memory_order_acquire);
			if( NULL != ins ) {
				static std::mutex __s_instance_for_destroy_mutex;
				__s_instance_for_destroy_mutex.lock();
				if (ins != NULL ) {
					delete ins;
					s_instance.store(NULL, std::memory_order_release);
				}
				__s_instance_for_destroy_mutex.unlock();
			}
		}

	protected:
		static void ScheduleForDestroy( void (*ptr_destroyFun)() ) {
			std::atexit( ptr_destroyFun ) ;
		}

	protected:
		Singleton() {}
		virtual ~Singleton() {}

		Singleton(const Singleton<T>&);
		Singleton<T>& operator=(const Singleton<T>&);

		static std::atomic<T*> s_instance;
	};

	template<class T>
	std::atomic<T*> Singleton<T>::s_instance(NULL);

#define DECLARE_SINGLETON_FRIEND(T) \
	friend class wawo::Singleton<T>
}

#endif//endof _WAWO_UTILS_SAFE_SINGLETION_H_
