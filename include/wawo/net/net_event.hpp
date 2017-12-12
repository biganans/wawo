#ifndef _WAWO_NET_NETEVENT_HPP_
#define _WAWO_NET_NETEVENT_HPP_

#include <wawo/net/event.hpp>

namespace wawo { namespace net {

	static const u8_t E_PACKET_ARRIVE	= 0;
	static const u8_t E_RD_SHUTDOWN		= 1;
	static const u8_t E_WR_SHUTDOWN		= 2;
	static const u8_t E_CLOSE			= 3;
	static const u8_t E_MESSAGE			= 4;
	static const u8_t E_WR_BLOCK		= 5;
	static const u8_t E_WR_UNBLOCK		= 6;
	static const u8_t E_ERROR			= 7;
	static const u8_t NE_MAX			= 8;

	class socket;
	struct socket_event :
		public event
	{
		WAWO_DECLARE_NONCOPYABLE(socket_event)
	public:

		WWRP<socket> so;
		explicit socket_event(u8_t const& id, WWRP<socket> const& so_, WWSP<packet> const& data, udata64 const& info_ = udata64(0)) :
			event(id,data,info_),
			so(so_)
		{
		}
		explicit socket_event(u8_t const& id, WWRP<socket> const& so_,udata64 const& info_ = udata64(0)) :
			event(id, NULL, info_),
			so(so_)
		{
		}
	};

	template <class _peer_t>
	struct peer_event :
		public event
	{
		WAWO_DECLARE_NONCOPYABLE(peer_event)

	public:
		typedef _peer_t peer_t;
		typedef typename _peer_t::message_t message_t;

		WWRP<peer_t> peer;
		WWRP<socket> so;
		WWSP<message_t> message; //incoming message

	public:
		explicit peer_event(u8_t const& id, WWRP<peer_t> const& peer_, WWRP<socket> const& so_, WWSP<message_t> const& message_) :
			event(id,NULL,udata64(0)),
			peer(peer_),
			so(so_),
			message(message_)
		{}
		explicit peer_event(u8_t const& id, WWRP<peer_t> const& peer_, WWRP<socket> const& so_, udata64 const& info_ = udata64(0)) :
			event(id, NULL, info_),
			peer(peer_),
			so(so_),
			message(NULL)
		{}
	};
}}
#endif