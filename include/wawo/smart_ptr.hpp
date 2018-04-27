#ifndef _WAWO_SMARTPTR_HPP_
#define _WAWO_SMARTPTR_HPP_

#include <wawo/config/compiler.h>
#include <wawo/basic.hpp>
#include <wawo/exception.hpp>

namespace wawo {

	template <class T>
	inline void checked_delete(T* p) {
		typedef char is_this_a_complete_type[sizeof(T)?1:-1];
		(void) sizeof(is_this_a_complete_type);
		delete p;
	}

	struct weak_ptr_lock_faild :
		public wawo::exception
	{
		weak_ptr_lock_faild():
			exception(wawo::E_MEMORY_ACCESS_ERROR,"weak_ptr_lock_failed", __FILE__, __LINE__,__FUNCTION__)
		{
		}
	};

	struct sp_counter_impl_malloc_failed:
		public wawo::exception
	{
		sp_counter_impl_malloc_failed():
			exception(wawo::E_MEMORY_ALLOC_FAILED, "sp_counter_impl_malloc_failed", __FILE__, __LINE__,__FUNCTION__)
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
	* 1, sizeof(wawo::shared_ptr) == 4, compared to sizeof(std::shared_ptr)==8
	* 2, safe store in stl's container
	*/

	class sp_counter_base {
	public:
		void* __p;
	private:
		std::atomic<long> sp_c;
		std::atomic<long> sp_weak_c;

		WAWO_DECLARE_NONCOPYABLE(sp_counter_base)
	public:
		sp_counter_base(void* const& p): __p(p),sp_c(1),sp_weak_c(1)
		{}

		virtual ~sp_counter_base() {}
		virtual void dispose() = 0;

		virtual void destroy() { delete this;}

		inline long sp_count() const { return sp_c.load(std::memory_order_acquire);}
		inline long weak_count() const { return sp_weak_c.load(std::memory_order_acquire);}
		inline void require() {wawo::atomic_increment(&sp_c);}
		inline long require_lock(long const& val) {
			return wawo::atomic_increment_if_not_equal(&sp_c,val,std::memory_order_acq_rel, std::memory_order_acquire);
		}
		inline void release() {
			if( wawo::atomic_decrement(&sp_c) == 1) {
				dispose();
				weak_release();
			}
		}
		void weak_require() { wawo::atomic_increment(&sp_weak_c);}
		void weak_release() {
			if( wawo::atomic_decrement(&sp_weak_c) == 1 ) {
				destroy();
			}
		}
	};

#if WAWO_ISWIN && (defined(_DEBUG) || defined(DEBUG) || 1)
	#define WAWO_ADD_PT_IN_IMPL_FOR_DEBUG
#endif

	template <class pt>
	class sp_counter_impl_p:
		public sp_counter_base
	{

#ifdef WAWO_ADD_PT_IN_IMPL_FOR_DEBUG
		pt* _pt;
#endif
		WAWO_DECLARE_NONCOPYABLE(sp_counter_impl_p)
		typedef sp_counter_impl_p<pt> this_type;
	public:
		explicit sp_counter_impl_p(pt* const& p): sp_counter_base(p)
#ifdef WAWO_ADD_PT_IN_IMPL_FOR_DEBUG
			,_pt(p)
#endif
		{}
		virtual void dispose() { wawo::checked_delete<pt>(static_cast<pt*>(__p)); __p=0;}
	};

	struct sp_weak_counter;
	struct sp_counter {
		sp_counter_base* base;

		sp_counter():
			base(0)
		{}

		template <class pt>
		explicit sp_counter(pt* const& p): base(0) {
			try {
				base = new sp_counter_impl_p<pt>(p);
			} catch(...) {
				wawo::checked_delete(base);
				throw wawo::sp_counter_impl_malloc_failed();
			}
		}

		~sp_counter() _WW_NOEXCEPT { if( base != 0) base->release();}

		sp_counter(sp_counter const& r) _WW_NOEXCEPT :
			base(r.base)
		{
			if( base != 0 ) base->require();
		}

		sp_counter(sp_counter&& r) _WW_NOEXCEPT:
			base(r.base)
		{
			r.reset();
		}

		sp_counter(sp_weak_counter const& weak);

		inline sp_counter& operator = (sp_counter const& r) _WW_NOEXCEPT {
			WAWO_ASSERT( base == r.base ? base == NULL: true );
			sp_counter(r).swap( *this );
			return (*this);
		}

		inline sp_counter& operator = (sp_counter&& r) _WW_NOEXCEPT {
			WAWO_ASSERT(base == r.base ? base == NULL : true);
			if (base != 0) base->release();
			base = r.base;
			r.reset();
			return (*this);
		}

		inline void swap( sp_counter& r ) _WW_NOEXCEPT {
			std::swap(base,r.base);
		}

		inline void reset() _WW_NOEXCEPT {
			base = 0;
		}

		inline long sp_count() const {
			if(base) return base->sp_count();
			return 0;
		}

		inline long weak_count() const {
			if (base) return base->sp_count();
			return 0;
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

	struct sp_weak_counter {
		sp_counter_base* base;

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

		void swap( sp_weak_counter& r ) _WW_NOEXCEPT {
			std::swap(base,r.base);
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
			WAWO_ASSERT( base->__p != 0 );
			WAWO_ASSERT( base->sp_count() > 0 );
		}
	}

	template <class T>
	class weak_ptr;

	template <class T>
	class shared_ptr {
	private:
		sp_counter sp_ct;//sp_counter

		friend class weak_ptr<T>;
		typedef T* POINTER_TYPE;
		typedef shared_ptr<T> THIS_TYPE ;
		typedef weak_ptr<T> WEAK_POINTER_TYPE;
	public:
		typedef T ELEMENT_TYPE;

		_WW_CONSTEXPR shared_ptr() _WW_NOEXCEPT:sp_ct() {}
		_WW_CONSTEXPR shared_ptr(std::nullptr_t) _WW_NOEXCEPT : sp_ct() {}

		~shared_ptr() _WW_NOEXCEPT {}
		long use_count() const _WW_NOEXCEPT { return sp_ct.sp_count(); }

		//_Tp_rel related type
		template <typename _Tp_rel>
		explicit shared_ptr(_Tp_rel* const& r) :
			sp_ct(r)
		{
			static_assert(std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value, "convertible check failed");
		}

		shared_ptr(THIS_TYPE const& r) _WW_NOEXCEPT:
			sp_ct(r.sp_ct)
		{}

		shared_ptr(THIS_TYPE&& r) _WW_NOEXCEPT :
			sp_ct(std::move(r.sp_ct))
		{}

		template <class _From>
		shared_ptr(shared_ptr<_From> const& r, ELEMENT_TYPE*) _WW_NOEXCEPT:
			sp_ct(r.sp_ct)
		{ //for cast
		}

		//_Tp_rel related type
		template <typename _Tp_rel
#ifdef _WW_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		shared_ptr( shared_ptr<_Tp_rel> const& r,typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0) _WW_NOEXCEPT
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type
		>
		shared_ptr( shared_ptr<_Tp_rel> const& r) _WW_NOEXCEPT
#endif
			:sp_ct(r.sp_ct)
		{
		}


		template <typename _Tp_rel
#ifdef _WW_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		shared_ptr(shared_ptr<_Tp_rel>&& r,typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0) _WW_NOEXCEPT
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type
		>
		shared_ptr( shared_ptr<_Tp_rel>&& r) _WW_NOEXCEPT
#endif
			: sp_ct(std::move(r.sp_ct))
		{
		}

		shared_ptr& operator=(THIS_TYPE const& r) _WW_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		shared_ptr& operator=(THIS_TYPE&& r) _WW_NOEXCEPT
		{
			sp_ct = std::move(r.sp_ct);
			return (*this);
		}

		template <typename _Tp_rel>
		shared_ptr& operator= ( shared_ptr<_Tp_rel> const& r ) _WW_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this) ;
		}

		template <typename _Tp_rel>
		shared_ptr& operator= (shared_ptr<_Tp_rel>&& r) _WW_NOEXCEPT
		{
			sp_ct = std::move(r.sp_ct);
			return (*this);
		}

		inline void swap( THIS_TYPE& r ) _WW_NOEXCEPT {
			std::swap( sp_ct, r.sp_ct );
		}

		shared_ptr(WEAK_POINTER_TYPE const& weak) _WW_NOEXCEPT:
			sp_ct(weak.weak_ct)
		{
		}

		inline POINTER_TYPE operator -> () const {
			WAWO_ASSERT( sp_ct.base != 0 );
			WAWO_ASSERT( sp_ct.base->__p != 0 );
			return static_cast<POINTER_TYPE>(sp_ct.base->__p);
		}

		inline POINTER_TYPE operator -> () {
			WAWO_ASSERT(sp_ct.base != 0);
			WAWO_ASSERT(sp_ct.base->__p != 0);
			return static_cast<POINTER_TYPE>(sp_ct.base->__p);
		}

		inline T const& operator * () const {
			WAWO_ASSERT(sp_ct.base != 0);
			WAWO_ASSERT(sp_ct.base->__p != 0);
			return *(static_cast<POINTER_TYPE>(sp_ct.base->__p));
		}

		inline T& operator * () {
			WAWO_ASSERT(sp_ct.base != 0);
			WAWO_ASSERT(sp_ct.base->__p != 0);
			return *(static_cast<POINTER_TYPE>(sp_ct.base->__p));
		}

		inline POINTER_TYPE get() const {
			if (sp_ct.base == 0) return 0;
			return static_cast<POINTER_TYPE>(sp_ct.base->__p);
		}

		inline bool operator ==(THIS_TYPE const& r) const { return sp_ct == r.sp_ct; }
		inline bool operator !=(THIS_TYPE const& r) const { return sp_ct != r.sp_ct; }

		inline bool operator < (THIS_TYPE const& r) const { return sp_ct < r.sp_ct; }
		inline bool operator > (THIS_TYPE const& r) const { return sp_ct > r.sp_ct; }

		inline bool operator ==(POINTER_TYPE const& rp) const { return get() == rp; }
		inline bool operator !=(POINTER_TYPE const& rp) const { return get() != rp; }

	private:
		template <class Y>
		friend class shared_ptr;
	};

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS

#define _ALLOCATE_MAKE_SHARED( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> inline \
	shared_ptr<_Ty> make_shared(LIST(_TYPE_REFREF_ARG)) \
	{	/* make a shared_ptr */ \
		_Ty *_Rx = \
		new _Ty(LIST(_FORWARD_ARG)); \
		WAWO_ALLOC_CHECK(_Rx); \
		return shared_ptr<_Ty>(_Rx); \
	}
_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_SHARED, , , , )
#undef _ALLOCATE_MAKE_SHARED
#else
	template <class _TTM, typename... _Args>
	inline shared_ptr<_TTM> make_shared(_Args&&... args)
	{
		_TTM* t = new _TTM(std::forward<_Args>(args)...);
		WAWO_ALLOC_CHECK(t,sizeof(_TTM));
		return shared_ptr<_TTM>(t);
	}
#endif


	/*
	 * test case for the following three functions

	 struct Base {
	 int i;
	 Base() :i(0) {}
	 Base(int ii) :i(ii) {}
	 virtual ~Base() {
	 std::cout << "Base(" << i << ") destruct" << std::endl;
	 }
	 };
	 struct Sub : public Base {
	 Sub() :Base(0) {}
	 Sub(int s) :Base(s) {}
	 };

	 std::shared_ptr<Sub> sub;
	 std::shared_ptr<Sub> sub_d;
	 std::shared_ptr<Base> base = std::make_shared<Sub>();
	 sub = std::static_pointer_cast<Sub, Base>(base);
	 sub_d = std::dynamic_pointer_cast<Sub, Base>(base);

	 std::shared_ptr<const Sub> csub;
	 csub = std::static_pointer_cast<Sub, Base>(base);
	 //csub->i = 10;

	 std::shared_ptr<Sub> csub_after_const_cast = std::const_pointer_cast<Sub,const Sub>(csub);
	 csub_after_const_cast->i = 10;


	 //std::shared_ptr<int> intx = std::make_shared<int>();
	 //sub_d = std::dynamic_pointer_cast<Sub, int>(intx);

	 //Sub* sub;
	 //Base* base = new Sub();
	 //sub = base;


	 struct Rep : public wawo::ref_base {
	 int i;
	 Rep() {}
	 ~Rep() {}
	 };
	 struct RepSub : public Rep {
	 ~RepSub() {};
	 };

	 wawo::shared_ptr<Sub> wsub;
	 wawo::shared_ptr<Sub> wsub_d;
	 wawo::shared_ptr<const Sub> wcsub;
	 wawo::shared_ptr<Base> wbase = wawo::make_shared<Sub>();
	 wsub = wawo::static_pointer_cast<Sub, Base>(wbase);
	 wsub_d = wawo::dynamic_pointer_cast<Sub, Base>(wbase);

	 wcsub = wawo::static_pointer_cast<Sub, Base>(wbase);
	 wawo::shared_ptr<Sub> wcsub_after_const_cast = wawo::const_pointer_cast<Sub, const Sub>(wcsub);
	 wcsub_after_const_cast->i = 10;

	 //wawo::shared_ptr<int> wintx = wawo::make_shared<int>();
	 //wsub_d = wawo::dynamic_pointer_cast<Sub, int>(wintx);
	 *
	 */

	template <class _To, class _From>
	inline shared_ptr<_To> static_pointer_cast(shared_ptr<_From> const& r )
	{
		//check castable
		typename shared_ptr<_To>::ELEMENT_TYPE* p = static_cast< typename shared_ptr<_To>::ELEMENT_TYPE* >( r.get() );
		return shared_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline shared_ptr<_To> dynamic_pointer_cast( shared_ptr<_From> const& r )
	{
		//check castable
		typename shared_ptr<_To>::ELEMENT_TYPE* p = dynamic_cast< typename shared_ptr<_To>::ELEMENT_TYPE* >( r.get() );
		if(p) return shared_ptr<_To>(r,p);
		return shared_ptr<_To>();
	}

	template <class _To, class _From>
	shared_ptr<_To> const_pointer_cast(shared_ptr<_From> const& r )
	{
		typename shared_ptr<_To>::ELEMENT_TYPE* p = const_cast< typename shared_ptr<_To>::ELEMENT_TYPE* >( r.get() );
		return shared_ptr<_To>(r,p);
	}

	template <class T>
	class weak_ptr {
		friend class shared_ptr<T>;
	private:
		sp_weak_counter weak_ct;
	public:
		typedef T ELEMENT_TYPE;
		typedef T* POINTER_TYPE;
		typedef weak_ptr<T> THIS_TYPE;
		typedef shared_ptr<T> SHARED_POINTER_TYPE;

		weak_ptr():weak_ct() {}
		explicit weak_ptr( SHARED_POINTER_TYPE const& auto_point ):
			weak_ct(auto_point.sp_ct)
		{
		}
		~weak_ptr() {}

		weak_ptr( THIS_TYPE const& r ):
			weak_ct(r.weak_ct)
		{
		}

		void swap( THIS_TYPE& r ) {
			std::swap(r.weak_ct,weak_ct);
		}

		THIS_TYPE& operator = (THIS_TYPE const& r) {
			weak_ptr(r).swap(*this);
			return *this;
		}

		THIS_TYPE& operator = (SHARED_POINTER_TYPE const& r) {
			weak_ptr(r).swap(*this);
			return *this;
		}

		//if lock failed, return a empty SharedPoint
		inline SHARED_POINTER_TYPE lock() const {
			return SHARED_POINTER_TYPE( *this );
		}

		inline bool operator == (THIS_TYPE const& r) const { return weak_ct == r.weak_ct; }
		inline bool operator != (THIS_TYPE const& r) const { return weak_ct != r.weak_ct; }
	};
}

//#define WAWO_SHARED_PTR wawo::shared_ptr
//#define WAWO_WEAK_PTR wawo::weak_ptr
#define WWSP wawo::shared_ptr
#define WWWP wawo::weak_ptr

#include <type_traits>

namespace wawo {

	template <class T>
	class ref_ptr {
		template <class Y>
		friend class ref_ptr;

		typedef T* POINTER_TYPE;
		typedef ref_ptr<T> THIS_TYPE;

		typename std::remove_cv<T>::type* _p;
	public:
		typedef T ELEMENT_TYPE;

		_WW_CONSTEXPR ref_ptr() _WW_NOEXCEPT:_p(0) {}
		_WW_CONSTEXPR ref_ptr(std::nullptr_t) _WW_NOEXCEPT : _p(0) {}

		~ref_ptr() { if (_p != 0) _p->_ref_drop(); }

		ref_ptr(THIS_TYPE const& r) _WW_NOEXCEPT :
			_p(r._p)
		{
			if (_p != 0) _p->_ref_grab();
		}

		ref_ptr(THIS_TYPE&& r) _WW_NOEXCEPT:
		_p(r._p)
		{
			r._p = 0;
		}

		template <class _From>
		ref_ptr(ref_ptr<_From> const& r, ELEMENT_TYPE* const p) _WW_NOEXCEPT:
		_p(p)
		{//for cast
			if (_p != 0) _p->_ref_grab();
			(void)r;
		}

		template <typename _Tp_rel>
		explicit ref_ptr(_Tp_rel* const& p)
			:_p(p)
		{
			static_assert(std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value, "convertible check failed");
			if (_p != 0) _p->_ref_grab();
		}

		template <typename _Tp_rel
#ifdef _WW_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		ref_ptr(ref_ptr<_Tp_rel> const& r, typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0 )
#else
		, class = typename std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type
		>
		ref_ptr( ref_ptr<_Tp_rel> const& r )
#endif
			:_p(r._p)
		{
			if(_p != 0) _p->_ref_grab();
		}

		template <typename _Tp_rel
#ifdef _WW_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
		>
		ref_ptr(ref_ptr<_Tp_rel>&& r, typename
			std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type ** = 0 )
#else
			, class = typename std::enable_if<std::is_convertible<_Tp_rel*, ELEMENT_TYPE*>::value>::type
		>
		ref_ptr(ref_ptr<_Tp_rel>&& r)
#endif
			:_p(r._p)
		{
			r._p = 0;
		}

		inline ref_ptr& operator= (THIS_TYPE const& r) _WW_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		inline ref_ptr& operator= (THIS_TYPE&& r) _WW_NOEXCEPT
		{
			if (_p != 0) _p->_ref_drop();
			_p = r._p;
			r._p = 0;
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator= (ref_ptr<_Tp_rel> const& r) _WW_NOEXCEPT
		{
			THIS_TYPE(r).swap(*this);
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator= (ref_ptr<_Tp_rel>&& r) _WW_NOEXCEPT
		{
			if (_p != 0) _p->_ref_drop();
			_p = r._p;
			r._p = 0;
			return (*this);
		}

		template <typename _Tp_rel>
		inline ref_ptr& operator = (_Tp_rel* const& p) _WW_NOEXCEPT
		{
			THIS_TYPE(p).swap(*this);
			return (*this);
		}

		inline void swap(THIS_TYPE& other) _WW_NOEXCEPT
		{
			std::swap( _p, other._p );
		}

		inline long ref_count() const _WW_NOEXCEPT { return (_p==0)?0:_p->_ref_count(); }

		inline T* const & operator -> () const
		{
			WAWO_ASSERT( _p != 0 );
			return (_p);
		}
		inline POINTER_TYPE& operator -> ()
		{
			WAWO_ASSERT(_p != 0);
			return (_p);
		}

		inline T const& operator * () const
		{
			WAWO_ASSERT( _p != 0 );
			return (*_p);
		}

		inline T& operator * ()
		{
			WAWO_ASSERT(_p != 0);
			return (*_p);
		}

		inline POINTER_TYPE get() const {return (_p);}

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
	 * Access ref_ptr<T> from multi-threads prior to keep a copy of ref_ptr<T> object
	 */

	class ref_base
	{
		WAWO_DECLARE_NONCOPYABLE(ref_base)
	private:
		std::atomic<long> __ref_c;
	protected:
		ref_base():__ref_c(0){}
		virtual ~ref_base(){}
	private:
		inline void _ref_grab() {
			WAWO_ASSERT(__ref_c.load(std::memory_order_acquire) >= 0);
			wawo::atomic_increment(&__ref_c,std::memory_order_acq_rel);
		}
		inline void _ref_drop() {
			WAWO_ASSERT(__ref_c.load(std::memory_order_acquire) >= 1);
			if(wawo::atomic_decrement(&__ref_c, std::memory_order_acq_rel) == 1) {
				delete this;
			}
		}
		inline long _ref_count() const { return __ref_c.load(std::memory_order_acquire); }

	private:
		void* operator new(std::size_t size) {
			WAWO_ASSERT(size!=0);
			return ::new char[size];
		}
		/*
		 @note
		* when overload operator delete with access level of private
		* on MSVC, it's OK
		* but
		* on GNUGCC, compiler give a access error for ~Sub calling a ~Parent
		* I don't know why. I'd appreciate if you could tell me !
		*/
	protected:
		void operator delete(void* p) {
			if (p != 0) {
				char* mem = reinterpret_cast<char*> (p);
				::delete[] mem;
			}
		}
	private:
#ifdef _WW_NO_CXX11_DELETED_FUNC
		void* operator new[](std::size_t size);
		void  operator delete[](void* p, std::size_t size);
#else
		void* operator new[](std::size_t size) = delete;
		void  operator delete[](void* p, std::size_t size) = delete;
#endif
		template <class T>
		friend class ref_ptr;

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_MAKE_REF( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> \
	friend inline ref_ptr<_Ty> make_ref(LIST(_TYPE_REFREF_ARG)) ;
_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_REF, , , ,)
#undef _ALLOCATE_MAKE_REF
#else
		template <class _TTM, typename... _Args>
		friend inline ref_ptr<_TTM> make_ref(_Args&&... args);
#endif
	};

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS
#define _ALLOCATE_MAKE_REF( \
	TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
template<class _Ty COMMA LIST(_CLASS_TYPE)> \
	inline ref_ptr<_Ty> make_ref(LIST(_TYPE_REFREF_ARG)) \
	{	/* make a shared_ptr */ \
		_Ty *_Rx = \
		new _Ty(LIST(_FORWARD_ARG)); \
		WAWO_ALLOC_CHECK(_Rx); \
		return (ref_ptr<_Ty>(_Rx)); \
	}
_VARIADIC_EXPAND_0X(_ALLOCATE_MAKE_REF, , , ,)
#undef _ALLOCATE_MAKE_REF
#else
	template <class _TTM, typename... _Args>
	inline ref_ptr<_TTM> make_ref(_Args&&... args)
	{
		_TTM* t = new _TTM(std::forward<_Args>(args)...);
		WAWO_ALLOC_CHECK(t,sizeof(_TTM));
		return ref_ptr<_TTM>(t);
	}
#endif

	/*
	 * test case for the following three function
		wawo::ref_ptr<RepSub> repsub;
		wawo::ref_ptr<RepSub> repsub_d;
		wawo::ref_ptr<Rep> rp = wawo::make_ref<RepSub>();

		repsub = wawo::static_pointer_cast<RepSub, Rep>(rp);
		repsub_d = wawo::dynamic_pointer_cast<RepSub, Rep>(rp);

		wawo::ref_ptr<const Rep> crep = rp;
		wawo::ref_ptr<Rep> crep_const_cast = wawo::const_pointer_cast<Rep, const Rep>(crep);
		crep_const_cast->i = 10;
	*/

	template <class _To, class _From>
	inline ref_ptr<_To> static_pointer_cast( ref_ptr<_From> const& r )
	{
		typename ref_ptr<_To>::ELEMENT_TYPE* p = static_cast< typename ref_ptr<_To>::ELEMENT_TYPE* >( r.get() );
		return ref_ptr<_To>(r,p);
	}

	template <class _To, class _From>
	inline ref_ptr<_To> dynamic_pointer_cast(ref_ptr<_From> const& r )
	{
		typename ref_ptr<_To>::ELEMENT_TYPE* p = dynamic_cast< typename ref_ptr<_To>::ELEMENT_TYPE* >( r.get() );
		if(p) return ref_ptr<_To>(r,p);
		return ref_ptr<_To>();
	}

	template <class _To, class _From>
	inline ref_ptr<_To> const_pointer_cast(ref_ptr<_From> const& r)
	{
		typename ref_ptr<_To>::ELEMENT_TYPE* p = const_cast< typename ref_ptr<_To>::ELEMENT_TYPE* >(r.get());
		return ref_ptr<_To>(r,p);
	}
}

//#define WAWO_REF_PTR wawo::ref_ptr
#define WWRP wawo::ref_ptr

#endif//_WAWO_SMARTPTR_HPP_