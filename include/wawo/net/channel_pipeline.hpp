#ifndef _WAWO_NET_CHANNEL_PIPELINE_HPP
#define _WAWO_NET_CHANNEL_PIPELINE_HPP


#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net {

	//class channel;
	class channel_pipeline :
		public ref_base,
		public channel_acceptor_invoker_abstract,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel;

	private:
		WWRP<channel> m_ch;
		WWRP<channel_handler_context> m_head;
		WWRP<channel_handler_context> m_tail;

	public:
		channel_pipeline(WWRP<channel> const& ch);
		virtual ~channel_pipeline();

		void init();
		void deinit();

		WWRP<channel_pipeline> add_last(WWRP<channel_handler_abstract> const& h) {
			WWRP<channel_handler_context> ctx = wawo::make_ref<channel_handler_context>( m_ch, h );
			
			m_tail->N = ctx;

			ctx->N = NULL;
			ctx->P = m_tail;

			m_tail = ctx;

			/*
			WWRP<channel_handler_context> tail_P = m_tail->P;
			ctx->N = m_tail;
			ctx->P = tail_P;
			tail_P->N = ctx;
			m_tail->P = ctx;
			*/
			return WWRP<channel_pipeline>(this);
		}

	protected:
		inline void fire_accepted( WWRP<channel> const& ch) {
			m_head->invoke_accepted( ch );
		}
		inline void fire_connected() {
			m_head->invoke_connected();
		}
		inline void fire_closed() {
			m_head->fire_closed();
		}
		inline void fire_error() {
			m_head->fire_error();
		}
		inline void fire_read_shutdowned() {
			m_head->invoke_read_shutdowned();
		}
		inline void fire_write_shutdowned() {
			m_head->invoke_write_shutdowned();
		}
		inline void fire_write_block() {
			m_head->invoke_write_block();
		}
		inline void fire_write_unblock() {
			m_head->invoke_write_unblock();
		}

		inline void fire_read(WWRP<packet> const& income) {
			m_head->fire_read(income);
		}

		inline int write(WWRP<packet> const& out) {
			WAWO_ASSERT(!"TODO");
			(void)out;
			return wawo::OK;
		}

		inline int close() {
			WAWO_ASSERT(!"TODO");
			return wawo::OK;
		}

		inline int close_read() {
			WAWO_ASSERT(!"TODO");
			return wawo::OK;
		}
		inline int close_write() {
			WAWO_ASSERT(!"TODO");
			return wawo::OK;
		}
	};
}}
#endif