#ifndef _WAWO_NET_CHANNEL_INVOKER_HPP
#define _WAWO_NET_CHANNEL_INVOKER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_future.hpp>

namespace wawo { namespace net {

	class channel;
	class channel_acceptor_invoker_abstract
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
		virtual void fire_read(WWRP<packet> const& income) = 0;
	};

	class channel_outbound_invoker_abstract
	{
	public:
		virtual WWRP<channel_future> write(WWRP<packet> const& out) = 0;
		virtual WWRP<channel_future> write(WWRP<packet> const& out, WWRP<channel_promise> const& ch_promise) = 0;

		virtual WWRP<channel_future> close() = 0;
		virtual WWRP<channel_future> close(WWRP<channel_promise> const& ch_promise) = 0;

		virtual WWRP<channel_future> shutdown_read() = 0;
		virtual WWRP<channel_future> shutdown_read(WWRP<channel_promise> const& ch_promise) = 0;

		virtual WWRP<channel_future> shutdown_write() = 0;
		virtual WWRP<channel_future> shutdown_write(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void flush() = 0;

//		virtual WWRP<channel_future> write_and_flush(WWRP<packet> const& out) = 0;
//		virtual WWRP<channel_future> write_and_flush(WWRP<packet> const& out, WWRP<channel_promise> const& ch_promise) = 0;
	};
}}
#endif