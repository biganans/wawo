#ifndef _WAWO_NET_SERVICE_POOL_HPP
#define _WAWO_NET_SERVICE_POOL_HPP

#include <wawo/thread/mutex.hpp>

namespace wawo {namespace net {

	typedef u8_t service_id_t;
	enum ServiceId {
		S_CUSTOM_ID_BEGIN = 10
	};

	using namespace wawo::thread;
	template <class _ServiceProvider>
	class service_pool {

	public:
		typedef _ServiceProvider SPT ;

	private:
		shared_mutex m_services_mutex;
		WWRP<SPT> m_services[wawo::limit::UINT8_MAX_V] ;
		
		void _reset() {
			for( int i=0;i<sizeof(m_services)/sizeof(m_services[0]);++i ) {
				m_services[i] = NULL ;
			}
		}

	public:
		service_pool() {
			_reset();
		}

		virtual ~service_pool() {
			_reset();
		}

		inline WWRP<SPT> get ( u32_t const& id ) {
			WAWO_ASSERT( id >=0 && id< (sizeof(m_services)/sizeof(m_services[0])) -1 );
			WAWO_ASSERT( m_services[id] != NULL );
			return m_services[id];
		}

		void add_service( u32_t const& id, WWRP<SPT> const& provider ) {
			lock_guard<shared_mutex> lg(m_services_mutex);

			WAWO_ASSERT(id >= 0 && id< (sizeof(m_services) / sizeof(m_services[0])) - 1);
			WAWO_ASSERT( m_services[id] == NULL );
			m_services[id] = provider;
		}

		void remove_service( u32_t const& id ) {
			lock_guard<shared_mutex> lg(m_services_mutex);

			WAWO_ASSERT(id >= 0 && id< (sizeof(m_services) / sizeof(m_services[0])) - 1);
			WAWO_ASSERT( m_services[id] != NULL );
			m_services[id] = NULL;
		}
	};
}}
#endif