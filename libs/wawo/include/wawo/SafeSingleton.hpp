#ifndef _WAWO_SAFE_SINGLETION_HPP_
#define _WAWO_SAFE_SINGLETION_HPP_

#include <thread>
#include <vector>
#include <mutex>

#include <wawo/core.h>

namespace wawo {

	template<class T>
	class SafeSingleton {

	public:
		inline static T*& GetInstance() {
			
			if ( NULL == s_instance ) {
				static std::mutex __s_instance_mutex;
				__s_instance_mutex.lock();

				if( s_instance == NULL ) {
					s_instance = new T();
					try {
						ScheduleForDestroy( SafeSingleton<T>::DestroyInstance );
					} catch( ... ) {
						//issue might be
						delete s_instance;
						s_instance = NULL;

						WAWO_ASSERT( !"ScheduleForDestroy failed!!!" );
					}
				}
				__s_instance_mutex.unlock();
			}

			return s_instance;
		}

		static void DestroyInstance() {
			if( s_instance != NULL ) {

				static std::mutex __s_instance_for_destroy_mutex;
				__s_instance_for_destroy_mutex.lock(); 

				if ( s_instance != NULL ) {
					delete s_instance ;
					s_instance = NULL;
				}

				__s_instance_for_destroy_mutex.unlock();
			}
		}

	protected:
		static void ScheduleForDestroy( void (*ptr_destroyFun)() ) {
			std::atexit( ptr_destroyFun ) ;
		}

	protected:
		SafeSingleton() {}
		virtual ~SafeSingleton() {}

		SafeSingleton(const SafeSingleton<T>&);
		SafeSingleton<T>& operator=(const SafeSingleton<T>&);

		static T* s_instance;
	};
	
	template<class T>
	T* SafeSingleton<T>::s_instance = NULL;

#define DECLARE_SAFE_SINGLETON_FRIEND(T) \
	friend class wawo::SafeSingleton<T>;
}

/*
namespace wawo { namespace utils {

	template<class T>
	class SafeSingletonForRefObject : public SafeSingleton<T> {

	public:	
		inline static T* GetInstance() {
			
			if ( NULL == SafeSingleton<T>::s_instance ) {

				static wawo::thread::Mutex __s_instance_mutex;

				__s_instance_mutex.Lock();
				if( SafeSingleton<T>::s_instance == NULL ) {
					SafeSingleton<T>::s_instance = new T();
					RefObject_Abstract *_ref_object_p = static_cast<Ref_Object_Abstract*>(SafeSingleton<T>::s_instance) ;
					_ref_object_p->Grab();
					try {
						SafeSingleton<T>::ScheduleForDestroy( SafeSingleton<T>::DestroyInstance );
					} catch( ... ) {
						//issue might be
						delete SafeSingleton<T>::s_instance;
						SafeSingleton<T>::s_instance = NULL;

						WAWO_ASSERT( !"ScheduleForDestroy failed!!!" );
					}
				}
				__s_instance_mutex.Unlock();
			}

			return SafeSingleton<T>::s_instance;
		}
	};
}}*/

#endif//endof _WAWO_UTILS_SAFE_SINGLETION_H_