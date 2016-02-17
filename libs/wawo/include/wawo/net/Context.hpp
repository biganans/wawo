#ifndef _WAWO_NET_CONTEXT_HPP
#define _WAWO_NET_CONTEXT_HPP

#include <wawo/SmartPtr.hpp>

namespace wawo { namespace net {

	template <class MyPeerT>
	struct SocketContext
	{
		WAWO_REF_PTR<MyPeerT> peer;
		int ec;
	};

	template <class _MyPeerT>
	struct MessageContext
	{
		typedef _MyPeerT MyPeerT;
		typedef typename _MyPeerT::MySocketT MySocketT;
		typedef typename _MyPeerT::MyMessageT MyMessageT;

		WAWO_REF_PTR<MyPeerT> peer;
		WAWO_REF_PTR<MySocketT> socket;
		WAWO_SHARED_PTR<MyMessageT> message;
	};
}}
#endif