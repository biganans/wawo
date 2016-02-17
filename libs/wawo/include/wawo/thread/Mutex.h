#ifndef _WAWO_THREAD_MUTEX_H_
#define _WAWO_THREAD_MUTEX_H_


#include <atomic>
#include <set>
#include <mutex>

#include <wawo/core.h>
#include <wawo/thread/Condition.h>

//#define USE_BOOST_SHARED_MUTEX
#ifdef USE_BOOST_SHARED_MUTEX
	#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef WAWO_PLATFORM_POSIX
	//default on
	#ifndef USE_PTHREAD_SPIN_MUTEX_AS_SPIN
		//#define USE_PTHREAD_SPIN_MUTEX_AS_SPIN
	#endif
#endif

#ifdef USE_PTHREAD_SPIN_MUTEX_AS_SPIN_MUTEX
	#include <pthread.h>
#endif

//#include <shared_mutex> //c++ 14 added, we use std::shared_mutex instead

#ifdef _DEBUG
	#define _DEBUG_MUTEX
	#define _DEBUG_SHARED_MUTEX
	#include <wawo/thread/Tls.hpp>
#endif

//#ifdef WAWO_PLATFORM_WIN
//#define USE_BOOL_FLAG
//#endif

namespace wawo { namespace thread {
	typedef std::set<ptrdiff_t> MutexSet ;

	template <class _MutexT>
	class UniqueLock;

	namespace _impl_ {

		class atomic_spin_mutex :
			public wawo::NonCopyable {
	#ifdef USE_BOOL_FLAG
				std::atomic_bool m_flag ; //ATOMIC_VAR_INIT(false);
	#else
		#ifdef WAWO_PLATFORM_POSIX
				std::atomic_flag m_flag = ATOMIC_FLAG_INIT; //force for sure
		#else
				std::atomic_flag m_flag ;
		#endif
	#endif
		public:
			atomic_spin_mutex();
			~atomic_spin_mutex();

			void lock();
			void unlock();
			bool try_lock();
		};

#ifdef WAWO_PLATFORM_POSIX
		class pthread_spin_mutex:
			public wawo::NonCopyable {
			private:
				pthread_spinlock_t m_spin;
			public:
				pthread_spin_mutex() {
					pthread_spin_init(&m_spin,0);
				}
				~pthread_spin_mutex() {
					pthread_spin_destroy(&m_spin);
				}
				void lock() {
					pthread_spin_lock(&m_spin);
				}
				void unlock(){
					pthread_spin_unlock(&m_spin);
				}
				bool try_lock() {
					return pthread_spin_trylock(&m_spin) == 0 ;
				}
		};
#endif

#ifdef USE_PTHREAD_SPIN_MUTEX_AS_SPIN_MUTEX
		typedef pthread_spin_mutex spin_mutex;
#else
		typedef atomic_spin_mutex spin_mutex;
#endif

		typedef std::mutex mutex;

		//pls be clear in mind when u decide to use this kind of mutex
		//hint, it's usually be used conjucted with lock for write, lock_shared for read

		template <class _StateMutexT>
		class shared_mutex
			: public wawo::NonCopyable
		{
			typedef _StateMutexT _MyStateMutexT;
			//the idea borrowed from boost
			class state_data {

			public:
				enum UpgradeState {
					S_UP_SET=1,
					S_UP_UPING,
					S_UP_NOSET
				};

				wawo::uint32_t shared_count;
				wawo::uint8_t upgrade;
				bool exclusive:8;

					state_data():
						shared_count(0),
						upgrade(S_UP_NOSET),
						exclusive(false)
					{}

					inline bool can_lock() const {
						return !(shared_count || exclusive);
					}
					inline void lock() {
						exclusive = true;
					}
					inline void unlock() {
						exclusive = false;
					}
					inline bool can_lock_shared() const {
						return !(exclusive || upgrade!=S_UP_NOSET);
					}
					inline bool has_more_shared() const {
						return shared_count>0;
					}
					inline uint32_t const& get_shared_count() const {
						return shared_count;
					}
					inline uint32_t const& lock_shared() {
						++shared_count;
						return shared_count;
					}
					inline void unlock_shared() {
						WAWO_ASSERT( !exclusive );
						WAWO_ASSERT( shared_count>0 );
						--shared_count ;
					}

					inline bool can_lock_upgrade() const {
						return !( exclusive || upgrade != S_UP_NOSET );
					}
					inline void lock_upgrade() {
						WAWO_ASSERT( !IsUpgradeSet() );
						++shared_count;
						upgrade=S_UP_SET;
					}
					inline void unlock_upgrade() {
						WAWO_ASSERT( upgrade == S_UP_SET );
						upgrade=S_UP_NOSET;
						WAWO_ASSERT( shared_count>0 );
						--shared_count;
					}
					inline bool IsUpgradeSet() const {
						return upgrade == S_UP_SET;
					}
					inline void unlock_upgrade_and_up() {
						WAWO_ASSERT( upgrade == S_UP_SET );
						upgrade=S_UP_UPING;
						WAWO_ASSERT( shared_count>0 );
						--shared_count;
					}
					inline void lock_upgrade_up_done() {
						WAWO_ASSERT( upgrade == S_UP_UPING );
						upgrade= S_UP_NOSET;
					}
				};

			state_data m_state;
			_MyStateMutexT m_state_mutex;

			wawo::thread::Condition m_shared_cond;
			wawo::thread::Condition m_exclusive_cond;
			wawo::thread::Condition m_upgrade_cond;

			inline void _notify_waiters() {
				m_exclusive_cond.notify_one();
				m_shared_cond.notify_all();
			}

		public:
			shared_mutex() {}
			~shared_mutex() {}

			void lock_shared() {
				UniqueLock<_MyStateMutexT> ulck( m_state_mutex );
				while( !m_state.can_lock_shared() ) {
					m_shared_cond.wait(ulck);
				}

				m_state.lock_shared();
			}
			void unlock_shared() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT( m_state.has_more_shared() );

				m_state.unlock_shared();
				if( !m_state.has_more_shared() ) {

					if( m_state.IsUpgradeSet() ) {
						//notify upgrade thread
						m_upgrade_cond.notify_one();
					} else {
						_notify_waiters();
					}
				}
			}
			void unlock_shared_and_lock() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT( m_state.has_more_shared() );
				WAWO_ASSERT( m_state.exclusive == false );

				m_state.unlock_shared();
				while( m_state.has_more_shared() || m_state.exclusive ) {
					m_exclusive_cond.wait(ulck);
				}
				WAWO_ASSERT( !m_state.has_more_shared());
				WAWO_ASSERT( m_state.exclusive==false );
				m_state.exclusive=true;
			}

			void lock() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				//upgrade count on has_more_shared
				while( m_state.has_more_shared() || m_state.exclusive ) {
					m_exclusive_cond.wait(ulck);
				}
				WAWO_ASSERT( !m_state.has_more_shared());
				WAWO_ASSERT( m_state.exclusive==false );
				m_state.exclusive=true;
			}
			void unlock() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT( !m_state.has_more_shared() );
				WAWO_ASSERT( m_state.exclusive == true );

				m_state.exclusive = false;
				_notify_waiters();
			}
			void unlock_and_lock_shared() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT( !m_state.has_more_shared() );
				WAWO_ASSERT( m_state.exclusive == true );

				m_state.exclusive = false;
				WAWO_ASSERT( m_state.can_lock_shared() );
				m_state.lock_shared();
			}

			bool try_lock() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				if( m_state.has_more_shared() || m_state.exclusive ) {
					return false;
				}

				m_state.exclusive = true;
				return true;
			}
			bool try_lock_shared() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);

				if( !m_state.can_lock_shared() ) {
					return false;
				}
				m_state.lock_shared();
				return true;
			}

			void lock_upgrade() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				while( !m_state.can_lock_upgrade() ) {
					m_shared_cond.wait(ulck);
				}
				m_state.lock_upgrade();
			}

			void unlock_upgrade() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				m_state.unlock_upgrade();
				if(m_state.has_more_shared()) {
					m_shared_cond.notify_all();
				} else {
					_notify_waiters();
				}
			}

			void unlock_upgrade_and_lock() {
				UniqueLock<_MyStateMutexT> ulck(m_state_mutex);
				m_state.unlock_upgrade_and_up();
				while( m_state.has_more_shared() || m_state.exclusive ) {
					m_upgrade_cond.wait(ulck);
				}

				WAWO_ASSERT( !m_state.has_more_shared() );
				WAWO_ASSERT( m_state.exclusive == false );
				m_state.exclusive=true;

				m_state.lock_upgrade_up_done();
			}
		};
	} //end of ns _impl_

	namespace _my_wrapper_ {
		class spin_mutex :
			public wawo::NonCopyable {
		private:
			_impl_::spin_mutex* m_impl;
		public:
			spin_mutex();
			~spin_mutex();

			void lock();
			void unlock();
			bool try_lock();
		};

		class mutex
			: public wawo::NonCopyable
		{
		public:
			mutex();
			~mutex();

			void lock();
			void unlock();
			bool try_lock();

		private:
			_impl_::mutex* m_impl;
		};

		template <class _MutexT>
		class shared_mutex
			: public wawo::NonCopyable
		{
			//pls be clear in mind when u decide to use this kind of mutex
			//hint, it's usually be used conjucted with lock for write, lock_shared for read
		public:
			shared_mutex() :
				m_impl(NULL)
			{
				m_impl = new _impl_::shared_mutex<_MutexT>() ;
				WAWO_NULL_POINT_CHECK( m_impl );
			}

			~shared_mutex() {
				WAWO_ASSERT( m_impl );

				WAWO_DELETE( m_impl );

	#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
	#endif
			}

			void lock_shared() {
				WAWO_ASSERT( m_impl );

		#ifdef _DEBUG_SHARED_MUTEX
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( mutex_set->find( _this ) == mutex_set->end()) ;
		#endif
				m_impl->lock_shared();

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->insert( _this );
		#endif
			}
			void unlock_shared() {
				WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( mutex_set->find( _this ) != mutex_set->end()) ;
		#endif
				m_impl->unlock_shared();

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->erase( _this );
		#endif
			}
			void unlock_shared_and_lock() {
				WAWO_ASSERT( m_impl != NULL );
		#ifdef _DEBUG_SHARED_MUTEX
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( mutex_set->find( _this ) != mutex_set->end()) ;
		#endif
				m_impl->unlock_shared_and_lock();

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->erase( _this );
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
				Tls<MutexSet>::Get()->insert( _this );
		#endif

			}

			void lock() {
				WAWO_ASSERT( m_impl != NULL );
		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
		#endif

				m_impl->lock();
		#ifdef _DEBUG_SHARED_MUTEX
				Tls<MutexSet>::Get()->insert( _this );
		#endif
			}
			void unlock() {
				WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) != Tls<MutexSet>::Get()->end()) ;
		#endif

				m_impl->unlock();

		#ifdef _DEBUG_SHARED_MUTEX
				Tls<MutexSet>::Get()->erase( _this );
		#endif
			}

			bool try_lock()  {
				WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
		#endif

				bool rt = m_impl->try_lock();

		#ifdef _DEBUG_SHARED_MUTEX
				if( rt == true ) {
					Tls<MutexSet>::Get()->insert( _this );
				}
		#endif

				return rt;
			}
			bool try_lock_shared() {
				WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) == Tls<MutexSet>::Get()->end()) ;
		#endif

				bool rt = m_impl->try_lock_shared();

		#ifdef _DEBUG_SHARED_MUTEX
				if( rt == true ){
					Tls<MutexSet>::Get()->insert( _this );
				}
		#endif

				return rt;
			}

			void lock_upgrade() {
				WAWO_ASSERT( m_impl );

		#ifdef _DEBUG_SHARED_MUTEX
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( mutex_set->find( _this ) == mutex_set->end()) ;
		#endif
				m_impl->lock_upgrade();

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->insert( _this );
		#endif
			}
			void unlock_upgrade() {
				WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( mutex_set->find( _this ) != mutex_set->end()) ;
		#endif
				m_impl->unlock_upgrade();

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->erase( _this );
		#endif
			}
			void unlock_upgrade_and_lock() {
			WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				WAWO_ASSERT( mutex_set->find( _this ) != mutex_set->end()) ;
		#endif

		#ifdef _DEBUG_SHARED_MUTEX
				mutex_set->erase( _this );
		#endif

				m_impl->unlock_upgrade_and_lock();
		#ifdef _DEBUG_SHARED_MUTEX
				Tls<MutexSet>::Get()->insert( _this );
		#endif

			}

			void unlock_and_lock_shared() {
			WAWO_ASSERT( m_impl != NULL );

		#ifdef _DEBUG_SHARED_MUTEX
				ptrdiff_t _this = (ptrdiff_t)(this);
				WAWO_ASSERT( Tls<MutexSet>::Get()->find( _this ) != Tls<MutexSet>::Get()->end()) ;
		#endif

				m_impl->unlock_and_lock_shared();

		#ifdef _DEBUG_SHARED_MUTEX
				Tls<MutexSet>::Get()->erase( _this );
				MutexSet* mutex_set = Tls<MutexSet>::Get() ;
				WAWO_ASSERT( mutex_set->find( _this ) == mutex_set->end()) ;
				mutex_set->insert( _this );
		#endif
			}

		private:
			_impl_::shared_mutex<_MutexT>* m_impl;
		};


	}
}}


namespace wawo { namespace thread {

#ifdef _DEBUG
	typedef wawo::thread::_my_wrapper_::spin_mutex SpinMutex;
//	typedef wawo::thread::_my_wrapper_::shared_mutex<_impl_::spin_mutex> SharedSpinMutex;

	#ifdef USE_BOOST_SHARED_MUTEX
		typedef boost::shared_mutex SharedMutex;
	#else
		typedef wawo::thread::_my_wrapper_::shared_mutex<_impl_::mutex> SharedMutex;
	#endif

	typedef wawo::thread::_my_wrapper_::mutex Mutex;
#else
	typedef wawo::thread::_impl_::spin_mutex SpinMutex;
	//typedef wawo::thread::_impl_::shared_mutex<_impl_::spin_mutex> SharedSpinMutex;

	#ifdef USE_BOOST_SHARED_MUTEX
		typedef boost::shared_mutex SharedMutex;
	#else
		typedef wawo::thread::_impl_::shared_mutex<_impl_::mutex> SharedMutex;
	#endif

	typedef wawo::thread::_impl_::mutex Mutex;
#endif

}}

namespace wawo { namespace thread {

		struct adopt_lock_t {};
		struct defer_lock_t {};
		struct try_to_lock_t {};

		extern const adopt_lock_t	adopt_lock;
		extern const defer_lock_t	defer_lock;
		extern const try_to_lock_t	try_to_lock;

		template <class _MutexT>
		class UniqueLock
			: public wawo::NonCopyable {
				typedef UniqueLock<_MutexT> UniqueLockType;
				typedef _MutexT MutexType;

		private:
			_MutexT& m_mtx;
			bool m_own;

		public:
			UniqueLock( _MutexT& mutex ):
				m_mtx(mutex),m_own(false)
			{
				m_mtx.lock();
				m_own = true;
			}
			~UniqueLock() {
				if( m_own ) {
					m_mtx.unlock();
				}
			}

			UniqueLock( _MutexT& mutex, adopt_lock_t ):
				m_mtx(mutex),m_own(true)
			{// construct and assume already locked
			}

			UniqueLock( _MutexT& mutex, defer_lock_t ):
				m_mtx(mutex),m_own(false)
			{// construct but don't lock
			}

			UniqueLock( _MutexT& mutex, try_to_lock_t ):
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
				return (m_own);
			}

			_MutexT* mutex() {
				return &m_mtx;
			}
		};

		template <class _MutexT>
		struct LockGuard
			: public wawo::NonCopyable
		{
			LockGuard( _MutexT& mtx ) : m_mtx(mtx) {
				m_mtx.lock();
			}

			~LockGuard() {
				m_mtx.unlock();
			}

			_MutexT& m_mtx;
		};

		template <class _MutexT>
		struct SharedLockGuard
			: public wawo::NonCopyable
		{
			SharedLockGuard(_MutexT& s_mtx):m_mtx(s_mtx) {
				m_mtx.lock_shared();
			}
			~SharedLockGuard() {
				m_mtx.unlock_shared() ;
 			}
			_MutexT& m_mtx;
		};

		template <class _MutexT>
		struct SharedUpgradeToLockGuard
			: public wawo::NonCopyable
		{
			SharedUpgradeToLockGuard(_MutexT& s_mtx):m_mtx(s_mtx) {
				m_mtx.unlock_shared_and_lock();
			}
			~SharedUpgradeToLockGuard() {
				m_mtx.unlock_and_lock_shared();
			}
			_MutexT& m_mtx;
		};
}}

#endif //_WAWO_THREAD_MUTEX_H_
