#ifndef _WAWO_NET_LISTENER_ABSTRACT_HPP_
#define _WAWO_NET_LISTENER_ABSTRACT_HPP_

#include <wawo/smart_ptr.hpp>
namespace wawo { namespace net {

	template <class E>
	class listener_abstract:
		virtual public wawo::ref_base
	{
	public:
		virtual ~listener_abstract() {} //FOR AUTO_TYPE
		virtual void on_event( WWRP<E> const& evt ) = 0 ;
	};

}}
#endif