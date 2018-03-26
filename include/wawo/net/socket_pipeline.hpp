#ifndef _WAWO_NET_SOCKET_PIPELINE_HPP
#define _WAWO_NET_SOCKET_PIPELINE_HPP


#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/socket_invoker.hpp>
#include <wawo/net/socket_handler.hpp>

namespace wawo { namespace net {

	class socket;
	class socket_pipeline :
		public ref_base,
		public socket_accept_invoker_abstract,
		public socket_activity_invoker_abstract,
		public socket_inbound_invoker_abstract,
		public socket_outbound_invoker_abstract
	{
		WWRP<socket> m_so;

		WWRP<socket_handler_context> m_head;
		WWRP<socket_handler_context> m_tail;

	public:
		socket_pipeline(WWRP<socket> const& so);
		virtual ~socket_pipeline();

		void init();
		void deinit();

		WWRP<socket_pipeline> add_last(WWRP<socket_handler_abstract> const& h) {
			WWRP<socket_handler_context> ctx = wawo::make_ref<socket_handler_context>( m_so, h );
			WWRP<socket_handler_context> tail_P = m_tail->P;

			ctx->N = m_tail;
			ctx->P = tail_P;
			tail_P->N = ctx;
			m_tail->P = ctx;

			return WWRP<socket_pipeline>(this);
		}

		virtual void fire_accepted( WWRP<socket> const& so) {
			m_head->invoke_accepted( so);
		}

		void fire_connected() {
			m_head->invoke_connected();
		}
		virtual void fire_closed() {
			m_head->fire_closed();
		}
		virtual void fire_error() {
			m_head->fire_error();
		}
		virtual void fire_read_shutdowned() {
			m_head->invoke_read_shutdowned();
		}
		virtual void fire_write_shutdowned() {
			m_head->invoke_write_shutdowned();
		}
		virtual void fire_write_block() {
			m_head->invoke_write_block();
		}
		virtual void fire_write_unblock() {
			m_head->invoke_write_unblock();
		}

		void fire_read(WWSP<packet> const& income) {
			m_head->fire_read(income);
		}

		void write(WWSP<packet> const& out) 
		{
			WAWO_ASSERT(!"TODO");
		}
	};
}}
#endif