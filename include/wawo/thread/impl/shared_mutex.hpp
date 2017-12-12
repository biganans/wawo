#ifndef WAWO_THREAD_IMPL_SHARD_MUTEX_HPP
#define WAWO_THREAD_IMPL_SHARD_MUTEX_HPP

#include <wawo/core.hpp>
#include <wawo/thread/condition.hpp>
#include <wawo/thread/impl/mutex_basic.hpp>

namespace wawo { namespace thread {

	namespace impl {
		
		template <class _StateMutexT>
		class shared_mutex
		{
			WAWO_DECLARE_NONCOPYABLE(shared_mutex)

			typedef _StateMutexT _MyStateMutexT;
			//the idea borrowed from boost
			class state_data {
			public:
				enum UpgradeState {
					S_UP_SET = 1,
					S_UP_UPING,
					S_UP_NOSET
				};

				wawo::u32_t shared_count;
				wawo::u8_t upgrade;
				bool exclusive : 8;

				state_data() :
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
					return !(exclusive || upgrade != S_UP_NOSET);
				}
				inline bool has_more_shared() const {
					return shared_count > 0;
				}
				inline u32_t const& get_shared_count() const {
					return shared_count;
				}
				inline u32_t const& lock_shared() {
					++shared_count;
					return shared_count;
				}
				inline void unlock_shared() {
					WAWO_ASSERT(!exclusive);
					WAWO_ASSERT(shared_count > 0);
					--shared_count;
				}

				inline bool can_lock_upgrade() const {
					return !(exclusive || upgrade != S_UP_NOSET);
				}
				inline void lock_upgrade() {
					WAWO_ASSERT(!is_upgrade_set());
					++shared_count;
					upgrade = S_UP_SET;
				}
				inline void unlock_upgrade() {
					WAWO_ASSERT(upgrade == S_UP_SET);
					upgrade = S_UP_NOSET;
					WAWO_ASSERT(shared_count > 0);
					--shared_count;
				}
				inline bool is_upgrade_set() const {
					return upgrade == S_UP_SET;
				}
				inline void unlock_upgrade_and_up() {
					WAWO_ASSERT(upgrade == S_UP_SET);
					upgrade = S_UP_UPING;
					WAWO_ASSERT(shared_count > 0);
					--shared_count;
				}
				inline void lock_upgrade_up_done() {
					WAWO_ASSERT(upgrade == S_UP_UPING);
					upgrade = S_UP_NOSET;
				}
			};

			state_data m_state;
			_MyStateMutexT m_state_mutex;

			wawo::thread::condition m_shared_cond;
			wawo::thread::condition m_exclusive_cond;
			wawo::thread::condition m_upgrade_cond;


			inline void _notify_waiters() {
				m_exclusive_cond.notify_one();
				m_shared_cond.notify_all();
			}

		public:
			shared_mutex() :
				m_state(),
				m_state_mutex(),
				m_shared_cond(),
				m_exclusive_cond(),
				m_upgrade_cond()
			{}
			~shared_mutex() {}

			void lock_shared() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				while (!m_state.can_lock_shared()) {
					m_shared_cond.wait(ulck);
				}

				m_state.lock_shared();
			}
			void unlock_shared() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT(m_state.has_more_shared());

				m_state.unlock_shared();
				if (!m_state.has_more_shared()) {

					if (m_state.is_upgrade_set()) {
						//notify upgrade thread
						m_upgrade_cond.notify_one();
					}
					else {
						_notify_waiters();
					}
				}
			}
			void unlock_shared_and_lock() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT(m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == false);

				m_state.unlock_shared();
				while (m_state.has_more_shared() || m_state.exclusive) {
					m_exclusive_cond.wait(ulck);
				}
				WAWO_ASSERT(!m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == false);
				m_state.exclusive = true;
			}

			void lock() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				//upgrade count on has_more_shared
				while (m_state.has_more_shared() || m_state.exclusive) {
					m_exclusive_cond.wait(ulck);
				}
				WAWO_ASSERT(!m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == false);
				m_state.exclusive = true;
			}
			void unlock() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT(!m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == true);

				m_state.exclusive = false;
				_notify_waiters();
			}
			void unlock_and_lock_shared() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				WAWO_ASSERT(!m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == true);

				m_state.exclusive = false;
				WAWO_ASSERT(m_state.can_lock_shared());
				m_state.lock_shared();
			}

			bool try_lock() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				if (m_state.has_more_shared() || m_state.exclusive) {
					return false;
				}

				m_state.exclusive = true;
				return true;
			}
			bool try_lock_shared() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);

				if (!m_state.can_lock_shared()) {
					return false;
				}
				m_state.lock_shared();
				return true;
			}

			void lock_upgrade() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				while (!m_state.can_lock_upgrade()) {
					m_shared_cond.wait(ulck);
				}
				m_state.lock_upgrade();
			}

			void unlock_upgrade() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				m_state.unlock_upgrade();
				if (m_state.has_more_shared()) {
					m_shared_cond.notify_all();
				}
				else {
					_notify_waiters();
				}
			}

			void unlock_upgrade_and_lock() {
				wawo::thread::unique_lock<_MyStateMutexT> ulck(m_state_mutex);
				m_state.unlock_upgrade_and_up();
				while (m_state.has_more_shared() || m_state.exclusive) {
					m_upgrade_cond.wait(ulck);
				}

				WAWO_ASSERT(!m_state.has_more_shared());
				WAWO_ASSERT(m_state.exclusive == false);
				m_state.exclusive = true;

				m_state.lock_upgrade_up_done();
			}
		};
	}//end of ns impl

	namespace _mutex_detail {
		template <class _MutexT>
		class shared_mutex
		{

		public:
			impl::shared_mutex<_MutexT> m_impl;
			WAWO_DECLARE_NONCOPYABLE(shared_mutex)
				//hint, it's usually be used conjucted with lock for write, lock_shared for read
		public:
			shared_mutex() :
				m_impl()
			{
			}

			~shared_mutex() {
				_MUTEX_DEBUG_CHECK_FOR_LOCK_
			}

			inline void lock_shared() {
				_MUTEX_IMPL_LOCK_SHARED(m_impl);
			}
			inline void unlock_shared() {
				_MUTEX_IMPL_UNLOCK_SHARED(m_impl);
			}
			inline void unlock_shared_and_lock() {
				_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
					m_impl.unlock_shared_and_lock();
				_MUTEX_DEBUG_LOCK_
			}

			inline void lock() {
				_MUTEX_IMPL_LOCK(m_impl);
			}
			inline void unlock() {
				_MUTEX_IMPL_UNLOCK(m_impl);
			}
			inline bool try_lock() {
				_MUTEX_IMPL_TRY_LOCK(m_impl);
			}
			inline bool try_lock_shared() {

				_MUTEX_DEBUG_CHECK_FOR_LOCK_
					bool rt = m_impl.try_lock_shared();
#ifdef _DEBUG_SHARED_MUTEX
				if (rt == true) {
					_MUTEX_DEBUG_LOCK_;
				}
#endif
				return rt;
			}

			inline void lock_upgrade() {
				_MUTEX_DEBUG_CHECK_FOR_LOCK_
					m_impl.lock_upgrade();
				_MUTEX_DEBUG_LOCK_
			}
			inline void unlock_upgrade() {
				_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
					m_impl.unlock_upgrade();
				_MUTEX_DEBUG_UNLOCK_
			}
			inline void unlock_upgrade_and_lock() {
				_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
					m_impl.unlock_upgrade_and_lock();
			}

			inline void unlock_and_lock_shared() {
				_MUTEX_DEBUG_CHECK_FOR_UNLOCK_
					m_impl.unlock_and_lock_shared();
				_MUTEX_DEBUG_LOCK_
			}

			inline impl::shared_mutex<_MutexT>* impl() { return (impl::shared_mutex<_MutexT>*)&m_impl; }
		};
	}
}}

namespace wawo { namespace thread {
#ifdef _WAWO_USE_MUTEX_WRAPPER
		typedef wawo::thread::_mutex_detail::shared_mutex<mutex> shared_mutex;
#else
		typedef wawo::thread::impl::shared_mutex<mutex> shared_mutex;
#endif
}}
#endif