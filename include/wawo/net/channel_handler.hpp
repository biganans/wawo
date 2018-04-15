#ifndef _WAWO_NET_CHANNEL_HANDLER_HPP
#define _WAWO_NET_CHANNEL_HANDLER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace net {

	class channel;
	class channel_handler_context;

	class channel_handler_abstract :
		virtual public ref_base
	{
	};

	class channel_activity_handler_abstract :
		virtual public channel_handler_abstract
	{
	public:
		virtual void connected(WWRP<channel_handler_context> const& ctx) ;
		virtual void closed(WWRP<channel_handler_context > const& ctx);
		virtual void error(WWRP<channel_handler_context> const& ctx);
		virtual void read_shutdowned(WWRP<channel_handler_context> const& ctx);
		virtual void write_shutdowned(WWRP<channel_handler_context> const& ctx);
		virtual void write_block(WWRP<channel_handler_context> const& ctx);
		virtual void write_unblock(WWRP<channel_handler_context> const& ctx);
	};

	class channel_acceptor_handler_abstract :
		virtual public channel_handler_abstract
	{
	public:
		virtual void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) = 0;
	};

	class channel_inbound_handler_abstract:
		virtual public channel_handler_abstract
	{
	public:
		virtual void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) = 0;
	};

	class channel_outbound_handler_abstract :
		virtual public channel_handler_abstract
	{
	public:
		virtual int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) = 0;

		virtual int close(WWRP<channel_handler_context> const& ctx,int const& code);
		virtual int close_read(WWRP<channel_handler_context> const& ctx,int const& code);
		virtual int close_write(WWRP<channel_handler_context> const& ctx,int const& code);
	};

#define HANDLER_DEFAULT_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx) { \
		ctx->fire_##NAME##(); \
	}

#define INT_HANDLER_DEFAULT_IMPL_INT_1(NAME,HANDLER_NAME) \
	int HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx, int const& code) { \
		return ctx->##NAME##(code); \
	}

	class channel_handler_head :
		public channel_activity_handler_abstract,
		public channel_acceptor_handler_abstract,
		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
	public:
		void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
		void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) ;
		int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) ;

		int close(WWRP<channel_handler_context> const& ctx, int const& code);
		int close_read(WWRP<channel_handler_context> const& ctx, int const& code);
		int close_write(WWRP<channel_handler_context> const& ctx, int const& code);
	};

	class channel_handler_tail :
		public channel_activity_handler_abstract,
		public channel_acceptor_handler_abstract,
		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
		void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
		void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) ;
		int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) ;
	};
}}
#endif