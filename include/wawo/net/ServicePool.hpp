#ifndef _WAWO_NET_SERVICE_POOL_HPP
#define _WAWO_NET_SERVICE_POOL_HPP

#include <wawo/thread/Mutex.hpp>

namespace wawo {namespace net {

	typedef u8_t ServiceIdT;
	enum ServiceId {
		S_CUSTOM_ID_BEGIN = 10
	};

	using namespace wawo::thread;
	template <class _ServiceProvider>
	class ServicePool {

		typedef _ServiceProvider SPT ;
		SharedMutex m_services_mutex;
		WWRP<SPT> m_services[wawo::limit::UINT8_MAX_V] ;
		
		void _Reset() {
			for( int i=0;i<sizeof(m_services)/sizeof(m_services[0]);i++ ) {
				m_services[i] = NULL ;
			}
		}

	public:
		ServicePool() {
			_Reset();
		}

		virtual ~ServicePool() {
			_Reset();
		}

		inline WWRP<SPT> Get ( u32_t const& id ) {
			WAWO_ASSERT( id >=0 && id< (sizeof(m_services)/sizeof(m_services[0])) -1 );
			WAWO_ASSERT( m_services[id] != NULL );
			return m_services[id];
		}

		void AddService( u32_t const& id, WWRP<SPT> const& provider ) {
			LockGuard<SharedMutex> lg(m_services_mutex);

			WAWO_ASSERT(id >= 0 && id< (sizeof(m_services) / sizeof(m_services[0])) - 1);
			WAWO_ASSERT( m_services[id] == NULL );
			m_services[id] = provider;
		}

		void RemoveService( u32_t const& id ) {
			LockGuard<SharedMutex> lg(m_services_mutex);

			WAWO_ASSERT(id >= 0 && id< (sizeof(m_services) / sizeof(m_services[0])) - 1);
			WAWO_ASSERT( m_services[id] != NULL );
			m_services[id] = NULL;
		}
	};
}}
#endif