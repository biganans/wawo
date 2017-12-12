#ifndef WAWO_THREAD_IMPL_MUTEX_BASIC_HPP
#define WAWO_THREAD_IMPL_MUTEX_BASIC_HPP

#include <wawo/macros.h>

//#define ENABLE_TRACE_MUTEX
#ifdef ENABLE_TRACE_MUTEX
	#include <wawo/log/logger_manager.h>
	#define TRACE_MUTEX WAWO_DEBUG
#else
	#define TRACE_MUTEX(...)
#endif

#define _WAWO_USE_MUTEX_WRAPPER

#if defined(_DEBUG) || defined(DEBUG)
	#define _DEBUG_MUTEX
	#define _DEBUG_SHARED_MUTEX
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
#include <set>
#include <wawo/thread/tls.hpp>
namespace wawo { namespace thread {
	typedef std::set<void*> MutexSet;
}}
#endif

#if defined(_DEBUG_MUTEX) || defined(_DEBUG_SHARED_MUTEX)
	#define _MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		do { \
			void*_this = (void*)(this); \
			MutexSet* mtxset = tls<MutexSet>::get(); \
			mtxset && (WAWO_ASSERT(mtxset->find(_this)==mtxset->end()),0); \
		} while (0);
	#define _MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		do { \
			void*_this = (void*)(this); \
			MutexSet* mtxset = tls<MutexSet>::get(); \
			mtxset && (WAWO_ASSERT(mtxset->find(_this)!=mtxset->end()),0); \
		} while (0);
	#define _MUTEX_DEBUG_LOCK_ \
		do { \
			void*_this = (void*)(this); \
			MutexSet* mtxset = tls<MutexSet>::get(); \
			mtxset && (mtxset->insert(_this),0); \
			TRACE_MUTEX("[lock]insert to MutexSet for: %p", _this ); \
		} while (0);

	#define _MUTEX_DEBUG_UNLOCK_ \
		do { \
			void*_this = (void*)(this); \
			MutexSet* mtxset = tls<MutexSet>::get(); \
			mtxset && (mtxset->erase(_this),0); \
			TRACE_MUTEX("[unlock]erase from MutexSet for: %p", _this ); \
		} while (0);
#else
	#define _MUTEX_DEBUG_CHECK_FOR_LOCK_
	#define _MUTEX_DEBUG_CHECK_FOR_UNLOCK_
	#define _MUTEX_DEBUG_LOCK_
	#define _MUTEX_DEBUG_UNLOCK_
#endif

#define _MUTEX_IMPL_LOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		impl.lock(); \
		_MUTEX_DEBUG_LOCK_ \
	} while (0);
#define _MUTEX_IMPL_UNLOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		impl.unlock(); \
		_MUTEX_DEBUG_UNLOCK_ \
	} while (0);

#define _MUTEX_IMPL_LOCK_SHARED(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		impl.lock_shared(); \
		_MUTEX_DEBUG_LOCK_ \
	} while (0);
#define _MUTEX_IMPL_UNLOCK_SHARED(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_UNLOCK_ \
		impl.unlock_shared(); \
		_MUTEX_DEBUG_UNLOCK_ \
	} while (0);

#define _MUTEX_IMPL_TRY_LOCK(impl) \
	do { \
		_MUTEX_DEBUG_CHECK_FOR_LOCK_ \
		bool b = impl.try_lock(); \
		if (true==b) { \
			_MUTEX_DEBUG_LOCK_ \
		} \
		return b; \
	} while (0);

namespace wawo { namespace thread {

	struct adopt_lock_t {};
	struct defer_lock_t {};
	struct try_to_lock_t {};

	extern const adopt_lock_t	adopt_lock;
	extern const defer_lock_t	defer_lock;
	extern const try_to_lock_t	try_to_lock;

	template <class _MutexT>
	class unique_lock
	{
		WAWO_DECLARE_NONCOPYABLE(unique_lock)

		typedef unique_lock<_MutexT> UniqueLockType;
		typedef _MutexT MutexType;

	private:
		_MutexT& m_mtx;
		bool m_own;

	public:
		unique_lock( _MutexT& mutex ):
			m_mtx(mutex),m_own(false)
		{
			m_mtx.lock();
			m_own = true;
		}
		~unique_lock() {
			if( m_own ) {
				m_mtx.unlock();
			}
		}

		unique_lock( _MutexT& mutex, adopt_lock_t ):
			m_mtx(mutex),m_own(true)
		{// construct and assume already locked
		}

		unique_lock( _MutexT& mutex, defer_lock_t ):
			m_mtx(mutex),m_own(false)
		{// construct but don't lock
		}

		unique_lock( _MutexT& mutex, try_to_lock_t ):
			m_mtx(mutex),m_own(m_mtx.try_lock())
		{
		}

		void lock() {
			WAWO_ASSERT( m_own == false );
			m_mtx.lock();
			m_own = true;
		}

		void unlock() {
			WAWO_ASSERT( m_own == true );
			m_mtx.unlock();
			m_own = false;
		}

		bool try_lock() {
			WAWO_ASSERT( m_own == false );
			m_own = m_mtx.try_lock();
			return m_own;
		}

		bool own_lock() {
			return m_own;
		}

		inline _MutexT const& mutex() const {
			return m_mtx;
		}

		inline _MutexT& mutex() {
			return m_mtx;
		}
	};

	template <class _MutexT>
	struct lock_guard
	{
		WAWO_DECLARE_NONCOPYABLE(lock_guard)
	public:
		lock_guard( _MutexT& mtx ) : m_mtx(mtx) {
			m_mtx.lock();
		}

		~lock_guard() {
			m_mtx.unlock();
		}

		_MutexT& m_mtx;
	};

	template <class _MutexT>
	struct shared_lock_guard
	{
		WAWO_DECLARE_NONCOPYABLE(shared_lock_guard)
	public:
		shared_lock_guard(_MutexT& s_mtx):m_mtx(s_mtx) {
			m_mtx.lock_shared();
		}
		~shared_lock_guard() {
			m_mtx.unlock_shared() ;
 		}
		_MutexT& m_mtx;
	};

	template <class _MutexT>
	struct shared_upgrade_to_lock_guard
	{
		WAWO_DECLARE_NONCOPYABLE(shared_upgrade_to_lock_guard)
	public:
		shared_upgrade_to_lock_guard(_MutexT& s_mtx):m_mtx(s_mtx) {
			m_mtx.unlock_shared_and_lock();
		}
		~shared_upgrade_to_lock_guard() {
			m_mtx.unlock_and_lock_shared();
		}
		_MutexT& m_mtx;
	};

}}
#endif
