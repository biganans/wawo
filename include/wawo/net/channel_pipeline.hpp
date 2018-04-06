#ifndef _WAWO_NET_CHANNEL_PIPELINE_HPP
#define _WAWO_NET_CHANNEL_PIPELINE_HPP


#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler.hpp>

namespace wawo { namespace net {

	//class channel;
	class channel_pipeline :
		public ref_base,
		public channel_accept_invoker_abstract,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel;
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
			WWRP<channel_handler_context> tail_P = m_tail->P;

			ctx->N = m_tail;
			ctx->P = tail_P;
			tail_P->N = ctx;
			m_tail->P = ctx;

			return WWRP<channel_pipeline>(this);
		}

	protected:
		void fire_accepted( WWRP<channel> const& so) {
			m_head->invoke_accepted( so);
		}
		void fire_connected() {
			m_head->invoke_connected();
		}
		void fire_closed() {
			m_head->fire_closed();
		}
		void fire_error() {
			m_head->fire_error();
		}
		void fire_read_shutdowned() {
			m_head->invoke_read_shutdowned();
		}
		void fire_write_shutdowned() {
			m_head->invoke_write_shutdowned();
		}
		void fire_write_block() {
			m_head->invoke_write_block();
		}
		void fire_write_unblock() {
			m_head->invoke_write_unblock();
		}

		void fire_read(WWSP<packet> const& income) {
			m_head->fire_read(income);
		}

		int write(WWSP<packet> const& out) {
			WAWO_ASSERT(!"TODO");
			(void)out;
			return wawo::OK;
		}

		int close(int const& code = 0) {
			WAWO_ASSERT(!"TODO");
			(void)code;
			return wawo::OK;
		}

		int close_read(int const& code =0) {
			WAWO_ASSERT(!"TODO");
			(void)code;
			return wawo::OK;
		}
		int close_write(int const& code = 0) {
			WAWO_ASSERT(!"TODO");
			(void)code;
			return wawo::OK;
		}
	};
}}
#endif