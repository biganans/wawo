#ifndef _WAWO_NET_CORE_LISTENER_ABSTRACT_HPP_
#define _WAWO_NET_CORE_LISTENER_ABSTRACT_HPP_

#include <wawo/SmartPtr.hpp>
namespace wawo { namespace net { namespace core {

	template <class E>
	class Listener_Abstract:
		virtual public wawo::RefObject_Abstract
	{
	public:
		virtual ~Listener_Abstract() {} //FOR AUTO_TYPE
		virtual void OnEvent( WAWO_REF_PTR<E> const& evt ) = 0 ;
	};

}}}
#endif
