#ifndef _WAWO_ALGORITHM_KV_POOL_HPP_
#define _WAWO_ALGORITHM_KV_POOL_HPP_


#include <map>
#include <vector>

namespace wawo { namespace algorithm {

	struct EC {
		enum _code_ {
			OK = 0,
			E_KV_POOL_K_EXISTS,
			E_KV_POOL_K_NOT_EXISTS
		};
	};

	template <class K, class V>
	struct KVPair {
		KVPair(K const& k, V const& v):
			k(k),
			v(v)
		{
		}
		K k;
		V v;
	};

	template <class K, class V>
	class KVPool {
	public:

		typedef KVPair<K,V> KVPair;
		typedef std::vector<KVPair> KVPairVector;
		typedef typename KVPairVector::iterator iterator;
		typedef typename KVPairVector::const_iterator const_iterator;


		KVPool( int default_init_capacity = 0 ) {
			if( default_init_capacity != 0 ) {
				Reserve( default_init_capacity );
			}
		}

		void Reserve( int capacity ) {
			m_kv_vector.reserve( capacity );
		}

		virtual ~KVPool() {
			m_kv_vector.clear();
		}

		void Clear() {
			m_kv_vector.clear();
		}

		int Insert( K const& k, V const& v ) {

			KVPairVector::iterator it = m_kv_vector.begin();

			while( it != m_kv_vector.end() ) {

				if( it->k == k ) {
					return EC::E_KV_POOL_K_EXISTS;
				}

				++it;
			}

			KVPair kv(k,v) ;
			m_kv_vector.push_back( kv );
			
			return EC::OK;
		}

		int Erase( K const&k ) {

			KVPairVector::iterator it = m_kv_vector.begin();

			while( it != m_kv_vector.end() ) {
				
				if( it->k == k ) {
					m_kv_vector.erase( it );
					return EC::OK ;
				}
			}
		
			return EC::E_KV_POOL_K_NOT_EXISTS ;
		}

		int Erase ( iterator const& it ) {
			return Erase( it->k );
		}

		int Size() {
			return m_kv_vector.size();
		}

		//don't define iterator const& ..., warning..
		iterator Begin() {
			return iterator(m_kv_vector.begin());
		}

		iterator End() {
			return iterator(m_kv_vector.end()) ;
		}

		iterator Find( K const&k ) {
			KVPairVector::iterator it = m_kv_vector.begin();

			while( it != m_kv_vector.end() ) {
				
				if( it->k == k ) {
					return iterator(it) ;
				}
				++it;
			}

			return iterator(m_kv_vector.end());
		}

		iterator Find( V const& v ) {
			KVPairVector::iterator it = m_kv_vector.begin();

			while( it != m_kv_vector.end() ) {
				
				if( it->v == v ) {
					return iterator(it) ;
				}
				++it;
			}

			return iterator(m_kv_vector.end());
		}

		bool Have( K const&k ) {
			return Find(k) != m_kv_vector.end() ;
		}

		bool Have( V const&v ) {
			KVPairVector::iterator it = m_kv_vector.begin();

			while( it != m_kv_vector.end() ) {
				
				if( it->v == v ) {
					return true ;
				}
			}
			return false;
		}
	private:
		KVPairVector m_kv_vector;
	};
}}
#endif