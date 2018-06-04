#ifndef _WAWO_NET_CHANNEL_HANDLER_HPP
#define _WAWO_NET_CHANNEL_HANDLER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_future.hpp>

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
		virtual void write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet, WWRP<channel_promise>& ch_promise ) = 0;

		virtual void close(WWRP<channel_handler_context> const& ctx, WWRP<channel_promise>& ch_promise);
		virtual void close_read(WWRP<channel_handler_context> const& ctx, WWRP<channel_promise>& ch_promise);
		virtual void close_write(WWRP<channel_handler_context> const& ctx, WWRP<channel_promise>& ch_promise);
		virtual void flush(WWRP<channel_handler_context> const& ctx);
	};

#define VOID_FIRE_HANDLER_DEFAULT_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx) { \
		ctx->fire_##NAME##(); \
	}

#define VOID_HANDLER_DEFAULT_IMPL_PROMISE(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx, WWRP<channel_promise>& ch_promise) { \
		ctx->##NAME##(ch_promise); \
	}

#define VOID_HANDLER_DEFAULT_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx) { \
		ctx->##NAME##(); \
	}

	class channel_handler_head :
//		public channel_activity_handler_abstract,
//		public channel_acceptor_handler_abstract,
//		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
	public:
		//void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
		//void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) ;
		int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) ;

		int close(WWRP<channel_handler_context> const& ctx);
		int close_read(WWRP<channel_handler_context> const& ctx);
		int close_write(WWRP<channel_handler_context> const& ctx);
	};

//	class channel_handler_tail :
//		public channel_activity_handler_abstract,
//		public channel_acceptor_handler_abstract,
//		public channel_inbound_handler_abstract
//		public channel_outbound_handler_abstract
//	{
		//void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
//		void read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) ;
		//int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) ;
//	};
}}
#endif