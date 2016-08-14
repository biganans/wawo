#ifndef _WAWO_THREAD_TLS_HPP_
#define _WAWO_THREAD_TLS_HPP_

#include <algorithm>
#include <thread>
#include <mutex>

#include <wawo/Singleton.hpp>

namespace wawo { namespace thread {

	template <class T>
	class TlsHolder {
		typedef std::vector<T*> Tlses;
		typedef typename Tlses::iterator _MyIterator;

	public:
		void PushBack( T* const t) {
			m_mutex.lock();
			m_tlses.push_back(t) ;
			m_mutex.unlock();
		}

		void EraseBack() {
			m_mutex.lock();
			delete m_tlses.back() ;
			m_tlses.pop_back();
			m_mutex.unlock();
		}

		void Erase( T* const t ) {
			m_mutex.lock();
			_MyIterator it = std::find(m_tlses.begin(), m_tlses.end(), t);
			WAWO_ASSERT( it != m_tlses.end() );
			if( it != m_tlses.end() ) {
				delete *it;
				m_tlses.erase(it);
			}
			m_mutex.unlock();
		}

		void Release() {
			while( m_tlses.size() ) {
				delete m_tlses.back() ;
				m_tlses.pop_back();
			}
		}
		TlsHolder() :
			m_mutex(),
			m_tlses()
		{}
		~TlsHolder() {
			Release();
		}

	private:
		std::mutex m_mutex;
		Tlses m_tlses;
	};


	template <class T>
	class Tls {

	public:
		static T* Get() {
			//tls_instance is thread_local , so dont lock
			if ( NULL == tls_instance ) {
					tls_instance = new T();
				try {
					Tls<T>::ScheduleForDestroy( tls_instance );
				} catch( ... ) {

					//issue might be
					delete tls_instance;
					tls_instance = NULL;

					WAWO_ASSERT( !"ScheduleForDestroy faile d!!!" );
				}
			}

			return tls_instance;
		}

	protected:
		static void ScheduleForDestroy( T* t ) {
			wawo::Singleton< TlsHolder<T> >::GetInstance()->PushBack(t);
		}
	protected:
		Tls() {}
		virtual ~Tls() {}

		Tls(const Tls<T>&);
		Tls<T>& operator=(const Tls<T>&);
		static WAWO_TLS T* tls_instance ;
	};

	template <class T>
	WAWO_TLS T* Tls<T>::tls_instance = NULL;

#define WAWO_DECLARE_TLS(T) \
	friend class wawo::thread::Tls<T>;

#define WAWO_TLS_INIT(T) wawo::Singleton< wawo::thread::TlsHolder<T> >::GetInstance();
#define WAWO_TLS_GET(T) wawo::thread::Tls<T>::Get()
#define WAWO_TLS_RELEASE(T) wawo::Singleton< wawo::thread::TlsHolder<T> >::GetInstance()->Erase( WAWO_TLS_GET(T) )


	//static wawo::thread::MutexSet& mutex_set = *(wawo::thread::Tls<wawo::thread::MutexSet>::Get());
}}
#endif
