#ifndef __WAWO___INIT___HPP_
#define __WAWO___INIT___HPP_

/////////////////////////////////////////
namespace __wawo__ {

	class __init__:
		public wawo::SafeSingleton<__init__>
	{
		DECLARE_SAFE_SINGLETON_FRIEND(__init__);
	protected:
		__init__() {

#ifdef _DEBUG
			//for mutex/lock deubg
			WAWO_TLS_INIT(wawo::thread::MutexSet) ;
#endif

		}
		~__init__() {}
	};
	static __init__& _ = *(__init__::GetInstance());
}
////////////////////////////////////////////
#endif