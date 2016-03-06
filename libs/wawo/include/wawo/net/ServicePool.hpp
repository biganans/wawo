#ifndef _WAWO_NET_SERVICE_POOL_HPP
#define _WAWO_NET_SERVICE_POOL_HPP

#include <wawo/thread/Mutex.h>

namespace wawo {namespace net {

	using namespace wawo::thread;

	template <class _ServiceProvider>
	class ServicePool {

		typedef _ServiceProvider MyServiceProviderT ;

		SharedMutex m_services_mutex;
		WAWO_REF_PTR<MyServiceProviderT> m_services[wawo::limit::UINT8_MAX_V] ;
		
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

		inline WAWO_REF_PTR<MyServiceProviderT> Get ( uint32_t const& id ) {
			WAWO_ASSERT( id >=0 && id< sizeof(m_services)/sizeof(m_services[0]) );
			return m_services[id];
		}

		void Register( uint32_t const& id, WAWO_REF_PTR<MyServiceProviderT> const& provider ) {
			LockGuard<SharedMutex> lg(m_services_mutex);

			WAWO_ASSERT( id >=0 && id< sizeof(m_services)/sizeof(m_services[0]) );
			WAWO_ASSERT( m_services[id] == NULL );
			m_services[id] = provider;
		}

		void UnRegister( uint32_t const& id ) {
			LockGuard<SharedMutex> lg(m_services_mutex);

			WAWO_ASSERT( id >=0 && id< sizeof(m_services)/sizeof(m_services[0]) );
			WAWO_ASSERT( m_services[id] != NULL );
			m_services[id] = NULL;
		}

	};

}}
#endif