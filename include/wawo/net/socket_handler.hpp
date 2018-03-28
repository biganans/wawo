#ifndef _WAWO_NET_SOCKET_HANDLER_HPP
#define _WAWO_NET_SOCKET_HANDLER_HPP

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

#include <wawo/net/socket_invoker.hpp>

namespace wawo { namespace net {

	class socket_handler_abstract :
		virtual public ref_base
	{
	};

	class socket;
	class socket_handler_context;

	class socket_activity_handler_abstract :
		virtual public socket_handler_abstract
	{
	public:
		virtual void connected(WWRP<socket_handler_context> const& ctx) ;
		virtual void closed(WWRP<socket_handler_context> const& ctx) ;
		virtual void error(WWRP<socket_handler_context> const& ctx);
		virtual void read_shutdowned(WWRP<socket_handler_context> const& ctx);
		virtual void write_shutdowned(WWRP<socket_handler_context> const& ctx);
		virtual void write_block(WWRP<socket_handler_context> const& ctx);
		virtual void write_unblock(WWRP<socket_handler_context> const& ctx);
	};

	class socket_accept_handler_abstract :
		virtual public socket_handler_abstract
	{
	public:
		virtual void accepted(WWRP<socket_handler_context> const& ctx, WWRP<socket> const& newsocket) = 0;
	};

	class socket_inbound_handler_abstract :
		virtual public socket_handler_abstract
	{
	public:
		virtual void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) = 0;
	};

	class socket_outbound_handler_abstract :
		virtual public socket_handler_abstract
	{
	public:
		virtual void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) = 0;

		virtual void close(WWRP<socket_handler_context> const& ctx,int const& code);
		virtual void close_read(WWRP<socket_handler_context> const& ctx,int const& code);
		virtual void close_write(WWRP<socket_handler_context> const& ctx,int const& code);
	};

	class socket_handler_context :
		public ref_base,
		public socket_accept_invoker_abstract,
		public socket_activity_invoker_abstract,
		public socket_inbound_invoker_abstract,
		public socket_outbound_invoker_abstract
	{
		enum handler_flag {
			F_ACCEPT	= 0x01,
			F_ACTIVITY	= 0x02,
			F_INBOUND	= 0x04,
			F_OUTBOUND	= 0x08,
		};

		WWRP<socket_handler_abstract> m_h;
		u8_t m_flag;
	public:

		WWRP<socket> so;
		WWRP<socket_handler_context> P;
		WWRP<socket_handler_context> N;

		socket_handler_context(WWRP<socket> const& so_, WWRP<socket_handler_abstract> const& h);
		virtual ~socket_handler_context();

		void fire_accepted( WWRP<socket> const& newso);
		void invoke_accepted( WWRP<socket> const& newso);

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

		void write(WWSP<packet> const& outlet);
		void invoke_write(WWSP<packet> const& outlet);

		void close(int const& code =0);
		void invoke_close(int const& code);

		void close_read(int const& code=0 );
		void invoke_close_read(int const& code );

		void close_write(int const& code=0 );
		void invoke_close_write(int const& code );
	};

	class socket_handler_head :
		public socket_activity_handler_abstract,
		public socket_accept_handler_abstract,
		public socket_inbound_handler_abstract,
		public socket_outbound_handler_abstract
	{
	public:
		void accepted(WWRP<socket_handler_context> const& ctx, WWRP<socket> const& newsocket) ;
		void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) ;
		void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) ;

		void close(WWRP<socket_handler_context> const& ctx, int const& code);
		void close_read(WWRP<socket_handler_context> const& ctx, int const& code);
		void close_write(WWRP<socket_handler_context> const& ctx, int const& code);
	};

	class socket_handler_tail :
		public socket_activity_handler_abstract,
		public socket_accept_handler_abstract,
		public socket_inbound_handler_abstract,
		public socket_outbound_handler_abstract
	{
		void accepted(WWRP<socket_handler_context> const& ctx, WWRP<socket> const& newsocket) ;
		void read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) ;
		void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) ;
	};
}}
#endif