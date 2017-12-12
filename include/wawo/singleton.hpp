#ifndef _WAWO_SINGLETON_HPP_
#define _WAWO_SINGLETON_HPP_

#include <mutex>
#include <wawo/core.hpp>
#include <wawo/exception.hpp>

namespace wawo {

	template <class T>
	class singleton {

	public:
		inline static T* instance() {

			T* ins = s_instance.load(std::memory_order_acquire);
			if ( WAWO_LIKELY(NULL != ins)) {
				return ins;
			}

			static std::mutex __s_instance_mutex;
			std::lock_guard<std::mutex> _lg(__s_instance_mutex);
			ins = s_instance.load(std::memory_order_acquire);
			if ( WAWO_LIKELY(NULL == ins)) {
				try {
					ins = new T();
					singleton<T>::schedule_for_destroy(singleton<T>::destroy_instance);
					s_instance.store(ins, std::memory_order_release);
				}
				catch (...) {
					//issue might be
					delete ins;
					WAWO_THROW("new T() | schedule_for_destroy failed!!!");
				}
			}
			return ins;
		}

		static void destroy_instance() {
			T* ins = s_instance.load(std::memory_order_acquire);
			if (NULL != ins) {
				static std::mutex __s_instance_for_destroy_mutex;
				std::lock_guard<std::mutex> lg(__s_instance_for_destroy_mutex);
				if (ins != NULL) {
					delete ins;
					s_instance.store(NULL, std::memory_order_release);
				}
			}
		}

	protected:
		static void schedule_for_destroy(void(*func)()) {
			std::atexit(func);
		}
	protected:
		singleton() {}
		virtual ~singleton() {}

		singleton(const singleton<T>&);
		singleton<T>& operator=(const singleton<T>&);

		static std::atomic<T*> s_instance;
	};

	template<class T>
	std::atomic<T*> singleton<T>::s_instance(NULL);

#define DECLARE_SINGLETON_FRIEND(T) \
	friend class wawo::singleton<T>
}

#endif//endof _WAWO_SINGLETION_HPP_