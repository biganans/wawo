#ifndef _WAWO_NET_SOCKET_INVOKER_HPP
#define _WAWO_NET_SOCKET_INVOKER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace net {

	class socket;
	
	class socket_accept_invoker_abstract
	{
		virtual void fire_accepted( WWRP<socket> const& so) = 0;
	};

	class socket_activity_invoker_abstract {
	public:
		virtual void fire_connected() = 0;
		virtual void fire_closed() = 0 ;
		virtual void fire_error() = 0 ;
		virtual void fire_read_shutdowned() = 0;
		virtual void fire_write_shutdowned() = 0;
		virtual void fire_write_block() = 0;
		virtual void fire_write_unblock() = 0;
	};

	class socket_inbound_invoker_abstract
	{
	public:
		virtual void fire_read(WWSP<packet> const& income) = 0;
	};

	class socket_outbound_invoker_abstract
	{
	public:
		virtual void write(WWSP<packet> const& out) = 0;
	};
}}
#endif