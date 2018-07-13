#ifndef _WAWO_TLS_HPP_
#define _WAWO_TLS_HPP_


#include <algorithm>
#include <thread>
#include <mutex>
#include <wawo/singleton.hpp>

namespace wawo {

	template <class T>
	class tls_instance_holder {
		typedef std::vector<T*> Tlses;
		typedef typename Tlses::iterator _MyIterator;

	public:
		void hold( T* const t) {
			std::lock_guard<std::mutex> lg(m_mutex);
			m_tlses.push_back(t) ;
		}

		void destroy_back() {
			std::lock_guard<std::mutex> lg(m_mutex);
			delete m_tlses.back() ;
			m_tlses.pop_back();
		}

		void destroy( T* const t ) {
			std::lock_guard<std::mutex> lg(m_mutex);
			_MyIterator it = std::find(m_tlses.begin(), m_tlses.end(), t);
			if( it != m_tlses.end() ) {
				delete *it;
				m_tlses.erase(it);
			}
		}

		void destroy_all() {
			std::lock_guard<std::mutex> lg(m_mutex);
			while( m_tlses.size() ) {
				delete m_tlses.back() ;
				m_tlses.pop_back();
			}
		}
		tls_instance_holder() :
			m_mutex(),
			m_tlses()
		{}
		~tls_instance_holder() {
			destroy_all();
		}

	private:
		std::mutex m_mutex;
		Tlses m_tlses;
	};


	template <class T>
	inline static void __schedule_for_destroy__(T* const t) {
		wawo::singleton< tls_instance_holder<T> >::instance()->hold(t);
	}
	template <class T>
	inline static void __destroy__(T* const t) {
		wawo::singleton< tls_instance_holder<T> >::instance()->destroy(t);
	}

	template <class T>
	class tls {
		WAWO_DECLARE_NONCOPYABLE(tls<T>)
		static __WAWO_TLS T* instance;

		tls() {}
		virtual ~tls() {}

	public:
#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS 

#define _ALLOCATE_CREATE_TLS_INSTANCE( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
	TEMPLATE_LIST(_CLASS_TYPE) \
	inline static T* create(LIST(_TYPE_REFREF_ARG)) \
	{ \
		T*_Rx = \
		new T(LIST(_FORWARD_ARG)); \
		instance = _Rx; \
		WAWO_ASSERT(_Rx != NULL); \
		__schedule_for_destroy__<T>(_Rx); \
		return (instance); \
	} \

_VARIADIC_EXPAND_0X(_ALLOCATE_CREATE_TLS_INSTANCE, , , , )
#undef _ALLOCATE_CREATE_TLS_INSTANCE

#else
		template <typename... Args>
		inline static T* create( Args&&... args ) {
			WAWO_ASSERT( NULL == instance );
			T* _instance(NULL);
			try {
				_instance = new T(std::forward<Args>(args)...);
				__schedule_for_destroy__<T>(_instance);
			} catch( ... ) {
				delete _instance;
				throw;
			}
			instance = _instance;
			WAWO_ASSERT( instance != NULL );
			return instance;
		}
#endif

		inline static T* get() {
			return instance;
		}
		inline static void set(T* const tins) {
			instance = tins;
		}
		inline static void schedule_destroy(T* const ins) {
			__schedule_for_destroy__<T>(ins);
		}
		inline static void destroy(T* const ins) {
			WAWO_ASSERT(instance == ins);
			instance = NULL;
			__destroy__<T>(ins);
		}
	};
	template <class T>
	__WAWO_TLS T* tls<T>::instance = NULL;

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_TLS_CREATE( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class T COMMA LIST(_CLASS_TYPE)> inline \
	T* tls_create(LIST(_TYPE_REFREF_ARG)) \
	{ \
		return tls<T>::create(LIST(_FORWARD_ARG)); \
	} \

_VARIADIC_EXPAND_0X(_ALLOCATE_TLS_CREATE, , , , )
#undef _ALLOCATE_TLS_CREATE

#else
	template <class T, typename... Args>
	inline T* tls_create(Args&&... args) {
		return tls<T>::create(std::forward<Args>(args)...);
	}
#endif

	template <class T>
	inline void tls_set(T* const instance) {
		tls<T>::set(instance);
	}

	template <class T>
	inline T* tls_get() {
		return tls<T>::get();
	}

	template <class T>
	inline void tls_destroy() {
		tls<T>::destroy(tls_get<T>());
	}

	template <class T>
	inline void tls_destroy(T* const instance) {
		tls<T>::destroy(instance);
	}

	template <class T>
	inline void tls_schedule_destroy(T* const instance) {
		tls<T>::schedule_destroy(instance);
	}
}
#endif