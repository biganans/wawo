#include <mutex>

#include <wawo/core.h>
#include <wawo/thread/Mutex.h>
#include <wawo/thread/Tls.hpp>

#ifdef _DEBUG
	#define _DEBUG_MUTEX
	#define _DEBUG_SHARED_MUTEX
#endif

namespace wawo { namespace thread {

	const adopt_lock_t		adopt_lock;
	const defer_lock_t		defer_lock;
	const try_to_lock_t		try_to_lock;

	namespace _impl_ {

		atomic_spin_mutex::atomic_spin_mutex()
#if defined(WAWO_PLATFORM_POSIX) && defined (USE_BOOL_FLAG)
		:m_flag(false)
#endif
		{

#ifdef _WAWO_CHECK_ATOMIC_BOOL
			std::atomic_bool ab;
			bool is_lock_free = ab.is_lock_free();
			std::atomic<bool> abb;
			bool is_lock_free_b = abb.is_lock_free();

			int sizeof_atomic_bool = sizeof( std::atomic_bool );
			int sizeof_atomic_bool_ins = sizeof( ab );

			int sizeof_atomic_bool_my_bool = sizeof( std::atomic<bool> );
			int sizeof_atomic_bool_my_bool_ins = sizeof( abb );
#endif

	#if defined(WAWO_PLATFORM_WIN) && defined(USE_BOOL_FLAG)
			std::atomic_init(&m_flag,false);
	#endif

	#if defined(WAWO_PLATFORM_WIN) && !defined(USE_BOOL_FLAG)
			const static std::atomic_flag flag = {0};
			m_flag = flag;
	#endif

	#ifdef USE_BOOL_FLAG
			WAWO_ASSERT(m_flag.load(std::memory_order_acquire) == false) ;
	#endif
		}
		atomic_spin_mutex::~atomic_spin_mutex() {
	#ifdef USE_BOOL_FLAG
			WAWO_ASSERT( m_flag.load(std::memory_order_acquire) == false );
	#endif
		}

		void atomic_spin_mutex::lock() {
	#ifdef USE_BOOL_FLAG
			while(m_flag.exchange(true, std::memory_order_acquire));
	#else
			for(unsigned k=0;!try_lock(); ++k) {
#ifdef WAWO_PLATFROM_POSIX
				//don't ask me why, 
				wawo::yield(k);
#endif
			}
	#endif
		}

		void atomic_spin_mutex::unlock() {
	#ifdef USE_BOOL_FLAG
			m_flag.store(false, std::memory_order_release);
	#else
			m_flag.clear(std::memory_order_release);
	#endif
		}

		bool atomic_spin_mutex::try_lock() {
	#ifdef USE_BOOL_FLAG
			bool f = false;
			bool t = true;
			return m_flag.compare_exchange_weak(f,t,std::memory_order_acquire,std::memory_order_acquire);
	#else
			return !m_flag.test_and_set(std::memory_order_acquire);
	#endif
		}

	} //endif _impl_

	namespace _my_wrapper_ {

		spin_mutex::spin_mutex():
			m_impl(NULL)
		{
			m_impl = new _impl_::spin_mutex();
			WAWO_NULL_POINT_CHECK(m_impl);
		}
		spin_mutex::~spin_mutex() {
			WAWO_ASSERT(m_impl != NULL);
			WAWO_DELETE(m_impl);
		}

		void spin_mutex::lock() {

	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
			WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif

			WAWO_ASSERT(m_impl != NULL);
			m_impl->lock();

	#ifdef _DEBUG_MUTEX
			Tls<MutexSet>::Get()->insert( _this );
	#endif
		}

		void spin_mutex::unlock() {

	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
	#endif

			WAWO_ASSERT(m_impl != NULL);
			m_impl->unlock();

	#ifdef _DEBUG_MUTEX
			Tls<MutexSet>::Get()->erase( _this );
	#endif
		}

		bool spin_mutex::try_lock() {
	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
			WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif

			WAWO_ASSERT(m_impl != NULL);
			bool b = m_impl->try_lock();

	#ifdef _DEBUG_MUTEX
			if( b == true ) {
				Tls<MutexSet>::Get()->insert( _this );
			}
	#else
			WAWO_THROW_EXCEPTION("TO IMPL");
	#endif
			return b;
		}

		mutex::mutex() :
			m_impl(NULL)
		{
			m_impl = new _impl_::mutex() ;
			WAWO_ASSERT( m_impl != NULL );
		}

		mutex::~mutex() {

			WAWO_ASSERT( m_impl != NULL );
			WAWO_DELETE( m_impl );

	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
			WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif

		}

		void mutex::lock() {
			WAWO_ASSERT( m_impl != NULL );

	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
			WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif
			m_impl->lock();

	#ifdef _DEBUG_MUTEX
			Tls<MutexSet>::Get()->insert( _this );
	#endif

		}

		void mutex::unlock() {
			WAWO_ASSERT( m_impl != NULL );

	#ifdef _DEBUG_MUTEX
	//condition wait will auto Unlock handler, and enable another thread to lock this, so , we can't check this
	//		std::string _current_thread_id_ = wawo::thread::Thread::GetThreadId() ;
	//		WAWO_ASSERT( !m_lock_thread_id.empty() && (m_lock_thread_id.compare(_current_thread_id_) == 0) );

			ptrdiff_t _this = (ptrdiff_t)(this);
	#endif
			m_impl->unlock();

	#ifdef _DEBUG_MUTEX
			Tls<MutexSet>::Get()->erase( _this );
	#endif
		}

		bool mutex::try_lock() {
			WAWO_ASSERT( m_impl );

	#ifdef _DEBUG_MUTEX
			ptrdiff_t _this = (ptrdiff_t)(this);
			WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif

			bool rt = m_impl->try_lock();
	#ifdef _DEBUG_MUTEX
			if( rt == true ) {
				Tls<MutexSet>::Get()->insert( _this );
			}
	#endif

			return rt;
		}
	
	} //end of _debug_wrapper_

}}
