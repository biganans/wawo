#ifndef _WAWO_SMARTPTR_HPP_
#define _WAWO_SMARTPTR_HPP_

#include <wawo/basic.hpp>

namespace wawo {

	template <class T>
	inline void checked_delete(T* p) {
		typedef char is_this_a_complete_type[sizeof(T)?1:-1];
		(void) sizeof(is_this_a_complete_type);
		delete p;
	}

	struct weak_ptr_lock_faild :
		public wawo::Exception
	{
		weak_ptr_lock_faild():
			Exception(wawo::E_MEMORY_ACCESS_ERROR,"weak_ptr_lock_failed", __FILE__, __FUNCTION__,__LINE__)
		{
		}
	};

	struct sp_counter_impl_malloc_failed:
		public wawo::Exception
	{
		sp_counter_impl_malloc_failed():
			Exception(wawo::E_MEMORY_ALLOC_FAILED, "sp_counter_impl_malloc_failed", __FILE__, __FUNCTION__,__LINE__)
		{}
	};
}

namespace wawo {

	/*
	 * usage
	 * if you want to make a object that would be shared but not be copyed, you need a ref object
	 * if you want to make a object that would be shared and it can be copyed, you need shared_ptr
	 *
	 */

	/*
	* two reason result in below classes
	* 1, sizeof(wawo::SharedPoint) == 4, compared to sizeof(std::shared_ptr)==8
	* 2, safe store in stl's container
	*/

	class sp_counter_base {
	private:
		sp_counter_base( sp_counter_base const& );
		sp_counter_base& operator = (sp_counter_base const&);

		std::atomic<i32_t> sp_c;
		std::atomic<i32_t> sp_weak_c;
	public:
		sp_counter_base(): sp_c(1),sp_weak_c(1)
		{}

		virtual ~sp_counter_base() {}
		virtual void* get_p() const = 0;
		virtual void dispose() = 0;

		virtual void destroy() {
			delete this;
		}

		i32_t sp_count() const {
			return sp_c.load(std::memory_order_acquire);
		}

		i32_t weak_count() const {
			return sp_weak_c.load(std::memory_order_acquire);
		}
		void require() {
			wawo::atomic_increment(&sp_c);
		}
		i32_t require_lock(i32_t const& val) {
			return wawo::atomic_increment_if_not_equal(&sp_c,val);
		}
		void release() {
			if( wawo::atomic_decrement(&sp_c) == 1) {
				dispose();
				weak_release();
			}
		}
		void weak_require() {
			wawo::atomic_increment(&sp_weak_c);
		}
		void weak_release() {
			if( wawo::atomic_decrement(&sp_weak_c) == 1 ) {
				destroy();
			}
		}
	};

	template <class pt>
	class sp_counter_impl_p:
		public sp_counter_base
	{
	private:
		typedef sp_counter_impl_p<pt> this_type;
		pt* _p;
		sp_counter_impl_p( sp_counter_impl_p const& );
		sp_counter_impl_p& operator=(sp_counter_impl_p const&);

	public:
		explicit sp_counter_impl_p(pt* p): _p(p)
		{}

		virtual void* get_p() const {return static_cast<void*>(_p);}
		virtual void dispose() { wawo::checked_delete<pt>(_p);_p=0;}
	};

	class sp_weak_counter;
	class sp_counter {

		friend class sp_weak_counter;
	private:
		sp_counter_base* base;
	public:
		sp_counter():
			base(0)
		{}

		template <class pt>
		explicit sp_counter(pt* p): base(0) {
			try {
				base = new sp_counter_impl_p<pt>(p);
			} catch(...) {
				wawo::checked_delete(base);
				throw wawo::sp_counter_impl_malloc_failed();
			}
		}

		~sp_counter() {
			//dcrese sp_counter_impl_p's sp_count
			if( base != 0) base->release();
		}

		sp_counter(sp_counter const& r):
			base(r.base)
		{
			if( base != 0 ) base->require();
		}

		sp_counter(sp_weak_counter const& weak);

		inline void* get_p() const {
			if( base == 0 ) return 0;
			return base->get_p();
		}

		inline sp_counter& operator = (sp_counter const& r) {
			WAWO_ASSERT( base != r.base );
			sp_counter(r).swap( *this );
			return *this;
		}

		inline void swap( sp_counter& r ) {
			wawo::swap(base,r.base);
		}

		inline i32_t sp_count() const {
			return base->sp_count();
		}

		inline i32_t weak_count() const {
			return base->weak_count();
		}

		inline bool operator == ( sp_counter const& r ) const {
			return base == r.base;
		}

		inline bool operator != ( sp_counter const& r ) const {
			return base != r.base;
		}

		inline bool operator < ( sp_counter const& r ) const {
			return base < r.base;
		}
		inline bool operator > ( sp_counter const& r ) const {
			return base > r.base;
		}
	};

	class sp_weak_counter {

		friend class sp_counter;
	private:
		sp_counter_base* base;

	public:
		sp_weak_counter(): base(0) {}
		sp_weak_counter( sp_weak_counter const& counter ):
			base(counter.base)
		{
			if( base != 0 ) base->weak_require();
		}

		sp_weak_counter( sp_counter const& counter ):
			base(counter.base)
		{
			if( base != 0 ) base->weak_require();
		}
		~sp_weak_counter() {
			if( base != 0 ) base->weak_release();
		}

		sp_weak_counter& operator = ( sp_weak_counter const& weak_counter ) {
			WAWO_ASSERT( base != weak_counter.base );
			sp_weak_counter(weak_counter).swap(*this);
			return *this;
		}

		sp_weak_counter& operator = (sp_counter const& sp_counter) {
			WAWO_ASSERT( base != sp_counter.base );
			sp_weak_counter(sp_counter).swap(*this);
			return *this;
		}

		void swap( sp_weak_counter& r ) {
			wawo::swap(base,r.base);
		}

		bool operator == ( sp_weak_counter const& r ) const {
			return base == r.base;
		}

		bool operator != ( sp_weak_counter const& r) const {
			return base != r.base;
		}
		bool operator < ( sp_weak_counter const& r) const {
			return base < r.base;
		}
		bool operator > ( sp_weak_counter const& r) const {
			return base > r.base;
		}
	};
	inline sp_counter::sp_counter(sp_weak_counter const& weak):
			base(weak.base)
	{
		if( base != 0 && base->require_lock(0) ) {
			WAWO_ASSERT( base->get_p() != 0 );
			WAWO_ASSERT( base->sp_count() > 0 );
		}
	}

	template <class T>
	class WeakPointer;

	template <class T>
	class SharedPointer {

		friend class WeakPointer<T>;

		typedef T* POINTER_TYPE;

		typedef SharedPointer<T> THIS_TYPE ;
		typedef WeakPointer<T> WEAK_POINTER_TYPE;
	private:
		sp_counter sp_ct;//sp_counter
	public:
		typedef T ELEMENT_TYPE;

		SharedPointer():sp_ct() {}
		SharedPointer(POINTER_TYPE const& point ):
			sp_ct(point)
		{
		}

		//dont make this line explicit ,,, compliler will call copy construct for ClassName o = o("xxx") ;
		SharedPointer( THIS_TYPE const& other ):
			sp_ct(other.sp_ct)
		{
		}

		//for static , dynamic cast
		template <class P>
		explicit SharedPointer( SharedPointer<P> const& r ):
			sp_ct(r.sp_ct)
		{
		}

		SharedPointer( WEAK_POINTER_TYPE const& weak ) :
			sp_ct(weak.weak_ct)
		{
		}

		~SharedPointer() {}

		i32_t SharedCount() const {
			return sp_ct.sp_count();
		}

		//I don't think we need this actually,
		// we can use AutoPoint<t> auto_pint( new PointType() ) instead this func
		THIS_TYPE& operator = (POINTER_TYPE const& p ) {
			THIS_TYPE(p).swap(*this);
			return *this;
		}

		THIS_TYPE& operator = ( THIS_TYPE const& r ) {
			THIS_TYPE(r).swap(*this);
			return *this ;
		}

		inline void swap( THIS_TYPE& other ) {
			wawo::swap( sp_ct, other.sp_ct );
		}

		inline POINTER_TYPE operator -> () const {
			POINTER_TYPE pt = static_cast<POINTER_TYPE>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return pt;
		}

		inline POINTER_TYPE operator -> () {
			POINTER_TYPE pt = static_cast<POINTER_TYPE>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return pt;
		}

		inline T const& operator * () const {
			POINTER_TYPE pt = static_cast<POINTER_TYPE>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return *pt;
		}

		inline T& operator * () {
			POINTER_TYPE pt = static_cast<POINTER_TYPE>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return *pt;
		}

		inline POINTER_TYPE Get() const {
			return static_cast<POINTER_TYPE>( sp_ct.get_p() );
		}

		inline bool operator ==(THIS_TYPE const& r) const { return sp_ct == r.sp_ct; }
		inline bool operator !=(THIS_TYPE const& r) const { return sp_ct != r.sp_ct; }

		inline bool operator < (THIS_TYPE const& r) const { return sp_ct < r.sp_ct; }
		inline bool operator > (THIS_TYPE const& r) const { return sp_ct > r.sp_ct; }

		inline bool operator ==(POINTER_TYPE const& rp) const { return Get() == rp; }
		inline bool operator !=(POINTER_TYPE const& rp) const { return Get() != rp; }

	private:
		template <class Y>
		friend class SharedPointer;
	};

	template <class T, class U>
	inline SharedPointer<T> static_pointer_cast(SharedPointer<U> const& r )
	{
		//check castable
		typename SharedPointer<T>::ELEMENT_TYPE* p = static_cast< typename SharedPointer<T>::ELEMENT_TYPE* >( r.Get() );
		return SharedPointer<T>(p);
	}

	template <class T, class U>
	inline SharedPointer<T> dynamic_pointer_cast( SharedPointer<U> const& r )
	{
		//check castable
		typename SharedPointer<T>::ELEMENT_TYPE* p = dynamic_cast< typename SharedPointer<T>::ELEMENT_TYPE* >( r.Get() );
		if(p) return SharedPointer<T>(p);
		return SharedPointer<T>();
	}

	//template <class T, class U>
	//SharedPoint<T> const_pointer_cast( SharedPoint<U> const& r )
	//{
	//	typename SharedPoint<T>::ELEMENT_TYPE* p = const_cast< typename SharedPoint<T>::ELEMENT_TYPE* >( r.Get() );
	//	return SharedPoint<T>(p);
	//}

	template <class T>
	class WeakPointer {
		friend class SharedPointer<T>;
	private:
		sp_weak_counter weak_ct;
	public:
		typedef T ELEMENT_TYPE;
		typedef T* POINTER_TYPE;
		typedef WeakPointer<T> THIS_TYPE;
		typedef SharedPointer<T> SHARED_POINTER_TYPE;

		WeakPointer():weak_ct() {}
		explicit WeakPointer( SHARED_POINTER_TYPE const& auto_point ):
			weak_ct(auto_point.sp_ct)
		{
		}
		~WeakPointer() {}

		WeakPointer( THIS_TYPE const& r ):
			weak_ct(r.weak_ct)
		{
		}

		void swap( THIS_TYPE& r ) {
			wawo::swap(r.weak_ct,weak_ct);
		}

		THIS_TYPE& operator = (THIS_TYPE const& r) {
			WeakPoint(r).swap(*this);
			return *this;
		}

		THIS_TYPE& operator = (SHARED_POINTER_TYPE const& r) {
			WeakPointer(r).swap(*this);
			return *this;
		}

		//if lock failed, return a empty SharedPoint
		inline SHARED_POINTER_TYPE Lock() const {
			return SHARED_POINTER_TYPE( *this );
		}

		inline bool operator == (THIS_TYPE const& r) const { return weak_ct == r.weak_ct; }
		inline bool operator != (THIS_TYPE const& r) const { return weak_ct != r.weak_ct; }
	};
}

#define WAWO_SHARED_PTR wawo::SharedPointer
#define WAWO_WEAK_PTR wawo::WeakPointer

#define WWSP wawo::SharedPointer
#define WWWP wawo::WeakPointer

namespace wawo {

	template <class T>
	class RefPointer {
		typedef T* POINTER_TYPE;
		typedef T* const POINTER_TYPE_CONST;
		typedef RefPointer<T> THIS_TYPE;
	private:
		POINTER_TYPE _p;
	public:

		typedef T ELEMENT_TYPE;

		RefPointer():_p(0) {}
		RefPointer( POINTER_TYPE const& p )
			:_p(p)
		{
			if(_p != 0) _p->Grab();
		}
		~RefPointer()
		{
			if(_p != 0) _p->Drop();
		}

		RefPointer(RefPointer const& r): _p(r._p)
		{
			if(_p != 0) _p->Grab();
		}

		RefPointer& operator = (RefPointer const& r)
		{
			THIS_TYPE(r).swap(*this);
			return *this;
		}

		RefPointer& operator = (POINTER_TYPE const& p)
		{
			THIS_TYPE(p).swap(*this);
			return *this;
		}

		void swap(THIS_TYPE& other)
		{
			wawo::swap( _p, other._p );
		}

		i32_t RefCount() const { return (_p==0)?0:_p->RefCount(); }

		inline POINTER_TYPE_CONST& operator -> () const
		{
			WAWO_ASSERT( _p != 0 );
			return _p;
		}
		inline POINTER_TYPE& operator -> ()
		{
			WAWO_ASSERT(_p != 0);
			return _p;
		}

		inline T const& operator * () const
		{
			WAWO_ASSERT( _p != 0 );
			return *_p;
		}

		inline T& operator * ()
		{
			WAWO_ASSERT(_p != 0);
			return *_p;
		}

		inline POINTER_TYPE Get() const {return _p;}

		inline bool operator == (THIS_TYPE const& r) const { return _p == r._p; }
		inline bool operator != (THIS_TYPE const& r) const { return _p != r._p; }
		inline bool operator > (THIS_TYPE const& r) const { return _p > r._p; }
		inline bool operator < (THIS_TYPE const& r) const { return _p < r._p; }

		inline bool operator == (POINTER_TYPE const& p) const { return _p == p; }
		inline bool operator != (POINTER_TYPE const& p) const { return _p != p; }
		inline bool operator > (POINTER_TYPE const& p) const { return _p > p; }
		inline bool operator < (POINTER_TYPE const& p) const { return _p < p; }
	};


	/**
	 * @Warning: the following behaviour is undefined
	 *
	 * Access RefPoint<T> from multi-threads prior to keep a copy of RefPoint<T> object
	 * call grab during object destruct member function
	 */

	class RefObject_Abstract :
		public wawo::NonCopyable
	{
		template <class T>
		friend class RefPointer;
	private:
		class ref_counter {
			friend class RefObject_Abstract;
		private:
			ref_counter( ref_counter const& );
			ref_counter& operator = ( ref_counter const& );
			std::atomic<i32_t> ref_c;
		protected:
			ref_counter(): ref_c(0) {}
			virtual ~ref_counter() {}
			inline i32_t ref_count() const {return ref_c;}
			inline void require() { WAWO_ASSERT(ref_c>=0); wawo::atomic_increment(&ref_c); }
			inline i32_t release() { return wawo::atomic_decrement(&ref_c); WAWO_ASSERT(ref_c >= 0); }
		};
		ref_counter counter;
	public:
		RefObject_Abstract():counter() {}
		virtual ~RefObject_Abstract() {}
	protected:
		inline void Grab() { return counter.require(); }
		inline void Drop() {
			if( counter.release() == 1) {
				delete this;
			}
		}
		inline i32_t RefCount() const { return counter.ref_count(); }
	};

	/*
	 * @Notice, I don't think we can impl a const_point_cast for Ref wrapper, or ,it's not a ref wrapper..
	 */

	template <class T, class U>
	inline RefPointer<T> static_pointer_cast( RefPointer<U> const& r )
	{
		typename RefPointer<T>::ELEMENT_TYPE* p = static_cast< typename RefPointer<T>::ELEMENT_TYPE* >( r.Get() );
		return RefPointer<T>(p);
	}

	template <class T, class U>
	inline RefPointer<T> dynamic_pointer_cast(RefPointer<U> const& r )
	{
		typename RefPointer<T>::ELEMENT_TYPE* p = dynamic_cast< typename RefPointer<T>::ELEMENT_TYPE* >( r.Get() );
		if(p) return RefPointer<T>(p);
		return RefPointer<T>();
	}
}

#define WAWO_REF_PTR wawo::RefPointer
#define WWRP wawo::RefPointer

#endif//_WAWO_UTILS_AUTOREFVAR_H_