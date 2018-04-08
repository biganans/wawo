#ifndef _WAWO_NET_CHANNEL_INVOKER_HPP
#define _WAWO_NET_CHANNEL_INVOKER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace net {

	class channel;
	class channel_accept_invoker_abstract
	{
		virtual void fire_accepted( WWRP<channel> const& ch) = 0;
	};

	class channel_activity_invoker_abstract {
	public:
		virtual void fire_connected() = 0;
		virtual void fire_closed() = 0 ;
		virtual void fire_error() = 0 ;
		virtual void fire_read_shutdowned() = 0;
		virtual void fire_write_shutdowned() = 0;
		virtual void fire_write_block() = 0;
		virtual void fire_write_unblock() = 0;
	};

	class channel_inbound_invoker_abstract
	{
	public:
		virtual void fire_read(WWSP<packet> const& income) = 0;
	};

	class channel_outbound_invoker_abstract
	{
	public:
		virtual int write(WWSP<packet> const& out) = 0;
		virtual int close(int const& code ) = 0;
		virtual int close_read(int const& code ) = 0;
		virtual int close_write(int const& code ) = 0;
	};
}}
#endif