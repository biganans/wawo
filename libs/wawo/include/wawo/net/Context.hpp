#ifndef _WAWO_NET_CONTEXT_HPP
#define _WAWO_NET_CONTEXT_HPP

#include <wawo/SmartPtr.hpp>

namespace wawo { namespace net {

	template <class _BasePeerT>
	struct BasePeerCtx 
	{
		WAWO_REF_PTR<_BasePeerT>						peer;
		WAWO_REF_PTR<typename _BasePeerT::MySocketT >	socket;
	};

	template <class _BasePeerT>
	struct BasePeerMessageCtx
	{
		WAWO_REF_PTR<_BasePeerT>						peer;
		WAWO_REF_PTR<typename _BasePeerT::MySocketT >	socket;
		WAWO_SHARED_PTR<typename _BasePeerT::MyMessageT> message;
	};

	template <class _MyPeerT>
	struct MyPeerCtx 
	{
		WAWO_REF_PTR<_MyPeerT> peer;
		WAWO_REF_PTR<typename _MyPeerT::MySocketT> socket;
		WAWO_SHARED_PTR<typename _MyPeerT::MyMessageT> message;
	};

}}
#endif