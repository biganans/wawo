#ifndef _WAWO_SMART_PTR_HPP_
#define _WAWO_SMART_PTR_HPP_

#include <wawo/core.h>

#define WAWO_DEBUG_REF_LOGIC
#define WAWO_USE_SPIN_LOCK

#ifndef NULL
	#define NULL 0
#endif

#ifdef _DEBUG
	#define _DEBUG_SMART_PTR
#endif

namespace wawo {
	template <class T> inline void checked_delete(T* p) {
		typedef char is_this_a_complete_type[sizeof(T)?1:-1];
		(void) sizeof(is_this_a_complete_type);
		delete p;
	}

	struct weak_ptr_lock_faild :
		public wawo::Exception
	{
		weak_ptr_lock_faild():
			wawo::Exception(wawo::E_MEMORY_ACCESS_ERROR,"weak_ptr_lock_failed", __FILE__, __FUNCTION__,__LINE__)
		{
		}
	};

	struct sp_counter_impl_malloc_failed:
		public wawo::Exception
	{
		sp_counter_impl_malloc_failed():
			wawo::Exception(wawo::E_MEMORY_ALLOC_FAILED, "sp_counter_impl_malloc_failed", __FILE__, __FUNCTION__,__LINE__)
		{}
	};
}


namespace wawo {


	/*
	 * usage
	 * for non_copy_able object, please extends RefObject_Abstract,,use wawo::RefPoint
	 * for copyable objects, please use wawo::SharedPoint
	 *
	 */


	/*
	* two reason result in below classes
	* 1, store in stl container is safe
	* 2, sizeof(wawo::SharedPoint) == 4, compared to sizeof(std::shared_ptr)==8
	*/

	class sp_counter_base {
	private:
		sp_counter_base( sp_counter_base const& );
		sp_counter_base& operator = (sp_counter_base const&);

		std::atomic<int32_t> sp_c;
		std::atomic<int32_t> sp_weak_c;
	public:
		sp_counter_base(): sp_c(1),sp_weak_c(1)
		{}

		virtual ~sp_counter_base() {}
		virtual void* get_p() = 0;
		virtual void dispose() = 0;

		virtual void destroy() {
			delete this;
		}

		int32_t sp_count() const {
			return sp_c.load(std::memory_order_acquire);
		}

		int32_t weak_count() const {
			return sp_weak_c.load(std::memory_order_acquire);
		}
		void require() {
			wawo::atomic_increment(&sp_c);
		}
		int32_t require_lock(int32_t const& val) {
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

		virtual void* get_p() {return static_cast<void*>(_p);}
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

		inline void* const get_p() const {
			if( base == 0 ) return 0;
			return base->get_p();
		}
		//just incre
		//void incre_sp_count() { base->require(); }
		//decre , and return previous
		//int32_t decre_sp_count() { return base->release() ;}

		sp_counter& operator = (sp_counter const& r) {
			WAWO_ASSERT( base != r.base );

			if( r.base != 0 ) r.base->require();
			if( base != 0 ) base->release();
			base = r.base;

			return *this;
		}

		void swap( sp_counter& r ) {
			wawo::swap(base,r.base);
		}

		int32_t sp_count() const {
			return base->sp_count();
		}

		int32_t weak_count() const {
			return base->weak_count();
		}

		bool operator == ( sp_counter const& r ) const {
			return base == r.base;
		}

		bool operator != ( sp_counter const& r ) const {
			return base != r.base;
		}

		bool operator < ( sp_counter const& r ) const {
			return base < r.base;
		}
		bool operator > ( sp_counter const& r ) const {
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

		sp_weak_counter& operator = ( sp_weak_counter const& counter ) {
			WAWO_ASSERT( base != counter.base );

			if( counter.base != 0 ) { counter.base->weak_require(); }
			if( base != 0 ) base->weak_release();
			base = counter.base;

			return *this;
		}

		sp_weak_counter& operator = (sp_counter const& counter) {
			WAWO_ASSERT( base != counter.base );

			if( counter.base != 0 ) { counter.base->weak_require(); }
			if( base != 0 ) base->weak_release();
			base = counter.base;

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

	//class sp_convert_helper {
	//public:
	//	template <class T, class Y>
	//	inline static T* pointer_y_to_t(Y* y) { return y; }
	//};

	//struct ap_no_grab_t {};
	template <class T>
	class WeakPoint;

	//class LoggerManager;
	template <class T>
	class SharedPoint {

		friend class WeakPoint<T>;

		typedef T* POINT_TYPE;
		typedef SharedPoint<T> THIS_TYPE ;
		typedef WeakPoint<T> WEAK_POINT_TYPE;
	private:
		sp_counter sp_ct;//sp_counter
	public:
		typedef T ELEMENT_TYPE;

		SharedPoint():sp_ct() {
		}

		SharedPoint( T* const& point ):
			sp_ct(point)
		{
		}

		//dont make this line explicit ,,, compliler will call copy construct for ClassName o = o("xxx") ;
		SharedPoint( THIS_TYPE const& other ):
			sp_ct(other.sp_ct)
		{
		}

		//for static , dynamic cast
		template <class P>
		explicit SharedPoint( SharedPoint<P> const& r ):
			sp_ct(r.sp_ct)
		{
		}

		SharedPoint( WEAK_POINT_TYPE const& weak ) :
			sp_ct(weak.weak_ct)
		{
		}

		~SharedPoint()
		{
		}

		int32_t SharedCount() const {
			return sp_ct.sp_count();
		}

		//I don't think we need this actually,
		// we can use AutoPoint<t> auto_pint( new PointType() ) instead this func
		THIS_TYPE& operator = ( T* const& p ) {
			THIS_TYPE(p).swap(*this);
			return *this;
		}

		THIS_TYPE& operator = ( THIS_TYPE const& r ) {
			THIS_TYPE(r).swap(*this);
			return *this ;
		}

		void swap( THIS_TYPE& other ) {
			wawo::swap(sp_ct, other.sp_ct);
		}

		inline T* operator -> () const {
			T* pt = static_cast<T*>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return pt;
		}

		inline T* operator -> () {
			T* pt = static_cast<T*>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return pt;
		}

		inline T& operator * () const {
			T* pt = static_cast<T*>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return *pt;
		}

		inline T& operator * () {
			T* pt = static_cast<T*>( sp_ct.get_p() );
			WAWO_ASSERT( pt != 0 );
			return *pt;
		}

		inline T* const Get() const {
			T* pt = static_cast<T*>( sp_ct.get_p() );
			return pt;
		}

		inline bool operator ==(THIS_TYPE const& r) const { return sp_ct == r.sp_ct; }
		inline bool operator !=(THIS_TYPE const& r) const { return sp_ct != r.sp_ct; }

		inline bool operator < (THIS_TYPE const& r) const { return sp_ct < r.sp_ct; }
		inline bool operator > (THIS_TYPE const& r) const { return sp_ct > r.sp_ct; }

		inline bool operator ==(T* const& rp) const { return Get() == rp; }
		inline bool operator !=(T* const& rp) const { return Get() != rp; }

	private:
		template <class Y>
		friend class SharedPoint;
	};

	template <class T, class U>
	SharedPoint<T> static_pointer_cast( SharedPoint<U> const& r )
	{
		//typename SharedPoint<T>::ELEMENT_TYPE* p = static_cast< typename SharedPoint<T>::ELEMENT_TYPE* >( r.Get() );
		return SharedPoint<T>(r);
	}

	//template <class T, class U>
	//SharedPoint<T> dynamic_pointer_cast( SharedPoint<U> const& r )
	//{
		//typename SharedPoint<T>::ELEMENT_TYPE* p = dynamic_cast< typename SharedPoint<T>::ELEMENT_TYPE* >( r.Get() );
	//	return SharedPoint<T>(p);
	//}

	//template <class T, class U>
	//SharedPoint<T> const_pointer_cast( SharedPoint<U> const& r )
	//{
	//	typename SharedPoint<T>::ELEMENT_TYPE* p = const_cast< typename SharedPoint<T>::ELEMENT_TYPE* >( r.Get() );
	//	return SharedPoint<T>(p);
	//}

	template <class T>
	class WeakPoint {
		friend class SharedPoint<T>;
	private:
		sp_weak_counter weak_ct;
	public:
		typedef T ELEMENT_TYPE;
		typedef T* POINT_TYPE;
		typedef WeakPoint<T> THIS_TYPE;
		typedef SharedPoint<T> SHARED_POINT_TYPE;

		WeakPoint():weak_ct() {}
		explicit WeakPoint( SHARED_POINT_TYPE const& auto_point ):
			weak_ct(auto_point.sp_ct)
		{
		}
		~WeakPoint() {
		}

		WeakPoint( THIS_TYPE const& r ):
			weak_ct(r.weak_ct)
		{
		}

		THIS_TYPE& operator = (THIS_TYPE const& r) {
			sp_weak_counter(r.weak_ct).swap(weak_ct);
			return *this;
		}

		THIS_TYPE& operator = (SHARED_POINT_TYPE const& r) {
			sp_weak_counter(r.sp_ct).swap(weak_ct);
			return *this;
		}

		//if lock failed, return a empty wawo::AutoPoint
		SHARED_POINT_TYPE Lock() const {
			return SHARED_POINT_TYPE( *this );
		}

		inline bool operator == (THIS_TYPE const& r) const { return weak_ct == r.weak_ct; }
		inline bool operator != (THIS_TYPE const& r) const { return weak_ct != r.weak_ct; }
	};
}

#define WAWO_SHARED_PTR wawo::SharedPoint
#define WAWO_WEAK_PTR wawo::WeakPoint

namespace wawo {

	template <class T>
	class RefPoint {
		typedef T* POINT_TYPE;
		typedef RefPoint<T> THIS_TYPE;
	private:
		POINT_TYPE _p;
	public:

		typedef T ELEMENT_TYPE;

		explicit RefPoint():_p(0) {}
		explicit RefPoint( POINT_TYPE const& p )
			:_p(p)
		{
			if(_p != 0) _p->Grab();
		}
		~RefPoint()
		{
			if(_p != 0) _p->Drop();
		}

		RefPoint( RefPoint const& r): _p(r._p)
		{
			if(_p != 0) _p->Grab();
		}

		RefPoint& operator = (RefPoint const& r)
		{
			THIS_TYPE(r).swap(*this);
			return *this;
		}

		RefPoint& operator = (POINT_TYPE const& p)
		{
			THIS_TYPE(p).swap(*this);
			return *this;
		}

		void swap(THIS_TYPE& other)
		{
			wawo::swap( _p, other._p );
		}

		int32_t RefCount() const { return (_p==0)?0:_p->RefCount(); }

		POINT_TYPE operator -> () const
		{
			WAWO_ASSERT( _p != 0 );
			return _p;
		}

		T& operator * ()
		{
			WAWO_ASSERT( _p != 0 );
			return *_p;
		}

		POINT_TYPE Get() const {return _p;}

		inline bool operator == (THIS_TYPE const& r) const { return _p == r._p; }
		inline bool operator != (THIS_TYPE const& r) const { return _p != r._p; }
		inline bool operator > (THIS_TYPE const& r) const { return _p > r._p; }
		inline bool operator < (THIS_TYPE const& r) const { return _p < r._p; }

		inline bool operator == (POINT_TYPE const& p) const { return _p == p; }
		inline bool operator != (POINT_TYPE const& p) const { return _p != p; }
		inline bool operator > (POINT_TYPE const& p) const { return _p > p; }
		inline bool operator < (POINT_TYPE const& p) const { return _p < p; }
	};

	/**
	 * thread security level
	 * you must hold one copy of AUTO_PTR FOR your objects before it is accessed across threads
	 *
	 * Warning: behaviour is undefined is u try to do Grab on ~self Destruct function
	 */

	class RefObject_Abstract : public wawo::NonCopyable
	{
	private:
		class ref_counter {
			friend class RefObject_Abstract;
		private:
			ref_counter( ref_counter const& );
			ref_counter& operator = ( ref_counter const& );

			std::atomic<int32_t> ref_c;

		protected:
			ref_counter(): ref_c(0)
			{}

			virtual ~ref_counter() {}
			int32_t ref_count() const {return ref_c;}
			void require() { wawo::atomic_increment(&ref_c); }
			int32_t release() { return wawo::atomic_decrement(&ref_c);}
		};
		ref_counter counter;
	public:
		RefObject_Abstract()
		{
		}
		virtual ~RefObject_Abstract() {
		}

		template <class T>
		friend class RefPoint;
	protected:
		void Grab() { return counter.require(); }
		void Drop() {
			if( counter.release() == 1) {
				delete this;
			}
		}
		int32_t RefCount() const { return counter.ref_count(); }
	};

	/*
	 * Notice, I think we can't impl a const_point_cast for Ref wrapper, or ,it's not a ref wrapper..
	 */

	template <class T, class U>
	RefPoint<T> static_pointer_cast( RefPoint<U> const& r )
	{
		typename RefPoint<T>::ELEMENT_TYPE* p = static_cast< typename RefPoint<T>::ELEMENT_TYPE* >( r.Get() );
		return RefPoint<T>(p);
	}

	template <class T, class U>
	RefPoint<T> dynamic_pointer_cast( RefPoint<U> const& r )
	{
		typename RefPoint<T>::ELEMENT_TYPE* p = dynamic_cast< typename RefPoint<T>::ELEMENT_TYPE* >( r.Get() );
		return RefPoint<T>(p);
	}
}

#define WAWO_REF_PTR wawo::RefPoint

#endif//_WAWO_UTILS_AUTOREFVAR_H_
