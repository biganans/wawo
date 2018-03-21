#ifndef _WAWO_NET_SOCKET_PIPELINE_HPP
#define _WAWO_NET_SOCKET_PIPELINE_HPP


#include <wawo/core.hpp>
#include <wawo/packet.hpp>
 
namespace wawo { namespace net {

	class activity_invoker_abstract {
		virtual void fire_connected() = 0;
		virtual void fire_closed() = 0;
		virtual void fire_read_shutdowned() = 0;
		virtual void fire_write_shutdowned() = 0;
		virtual void fire_write_block() = 0 ;
		virtual void fire_write_unblock() = 0;
	};

	class inbound_invoker_abstract
	{
		virtual void fire_arrive(WWSP<packet> const& income ) = 0;
	};

	class outbound_invoker_abstract
	{
		virtual void write(WWSP<packet> const& out ) = 0;
	};

	class socket_handler_context_base
	{
	public:
		u8_t m_type;
		socket_handler_context_base(u8_t const& t)
			:m_type(t)
		{
		}
	};

	class socket_handler_context :
		public ref_base,
		public socket_handler_context_base
	{

	};

	class socket_handler_abstract
	{
	};

	class socket_activity_handler_abstract:
		virtual public socket_handler_abstract
	{
		virtual void connected(WWRP<socket_handler_context> const& ctx) = 0;
		virtual void closed(WWRP<socket_handler_context> const& ctx) = 0;
		virtual void read_shutdowned(WWRP<socket_handler_context> const& ctx) = 0;
		virtual void write_shutdowned(WWRP<socket_handler_context> const& ctx) = 0;
		virtual void write_block(WWRP<socket_handler_context> const& ctx) = 0;
		virtual void write_unblock(WWRP<socket_handler_context> const& ctx) = 0;
	};

	class socket_inbound_handler_abstract :
		virtual public socket_handler_abstract
	{
		virtual void arrive(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income ) = 0;
	};

	class socket_outbound_handler_abstract :
		virtual public socket_handler_abstract
	{
		virtual void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) = 0;
	};

	enum handler_type {
		T_ACTIVITY,
		T_INBOUND,
		T_OUTBOND
	};

	class pipeline:		
		public ref_base,
		public activity_invoker,
		public inbound_invoker,
		public outbound_invoker
	{
	public:




	};
}}
#endif