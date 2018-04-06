#ifndef _WAWO_NET_CHANNEL_HANDLER_HPP
#define _WAWO_NET_CHANNEL_HANDLER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

#include <wawo/net/channel_invoker.hpp>

namespace wawo { namespace net {

	class channel_handler_abstract :
		virtual public ref_base
	{
	};

	class channel;
	class channel_handler_context;

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

	class channel_accept_handler_abstract :
		virtual public channel_handler_abstract
	{
	public:
		virtual void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) = 0;
	};

	class channel_inbound_handler_abstract:
		virtual public channel_handler_abstract
	{
	public:
		virtual void read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income) = 0;
	};

	class channel_outbound_handler_abstract :
		virtual public channel_handler_abstract
	{
	public:
		virtual int write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet) = 0;

		virtual int close(WWRP<channel_handler_context> const& ctx,int const& code);
		virtual int close_read(WWRP<channel_handler_context> const& ctx,int const& code);
		virtual int close_write(WWRP<channel_handler_context> const& ctx,int const& code);
	};

	class channel_handler_context :
		public ref_base,
		public channel_accept_invoker_abstract,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		enum handler_flag {
			F_ACCEPT	= 0x01,
			F_ACTIVITY	= 0x02,
			F_INBOUND	= 0x04,
			F_OUTBOUND	= 0x08,
		};

		WWRP<channel_handler_abstract> m_h;
		u8_t m_flag;
	public:

		WWRP<channel> ch;
		WWRP<channel_handler_context> P;
		WWRP<channel_handler_context> N;

		channel_handler_context(WWRP<channel> const& ch, WWRP<channel_handler_abstract> const& h);
		virtual ~channel_handler_context();

		void fire_accepted( WWRP<channel> const& newso);
		void invoke_accepted( WWRP<channel> const& newso);

		void fire_connected();
		void invoke_connected();
		
		void fire_closed();
		void invoke_closed();

		void fire_error();
		void invoke_error();

		void fire_read_shutdowned() ;
		void invoke_read_shutdowned();

		void fire_write_shutdowned() ;
		void invoke_write_shutdowned();

		void fire_write_block() ;
		void invoke_write_block();

		void fire_write_unblock();
		void invoke_write_unblock();

		void fire_read(WWSP<packet> const& income);
		void invoke_read(WWSP<packet> const& income);

		int write(WWSP<packet> const& outlet);
		int invoke_write(WWSP<packet> const& outlet);

		int close(int const& code =0);
		int invoke_close(int const& code);

		int close_read(int const& code=0 );
		int invoke_close_read(int const& code );

		int close_write(int const& code=0 );
		int invoke_close_write(int const& code );
	};

	class channel_handler_head :
		public channel_activity_handler_abstract,
		public channel_accept_handler_abstract,
		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
	public:
		void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
		void read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income) ;
		int write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet) ;

		int close(WWRP<channel_handler_context> const& ctx, int const& code);
		int close_read(WWRP<channel_handler_context> const& ctx, int const& code);
		int close_write(WWRP<channel_handler_context> const& ctx, int const& code);
	};

	class socket_handler_tail :
		public channel_activity_handler_abstract,
		public channel_accept_handler_abstract,
		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
		void accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch) ;
		void read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income) ;
		int write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet) ;
	};
}}
#endif