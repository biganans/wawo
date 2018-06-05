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
		WWRP<channel_pipeline> add_last(WWRP<channel_handler_abstract> const& h);

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

		inline WWRP<channel_future> write(WWRP<packet> const& out) {
			WAWO_ASSERT(!"TODO");
			(void)out;
			return NULL;
		}

		inline WWRP<channel_future> write(WWRP<packet> const& out, WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(!"TODO");
			(void)out;
			(void)ch_promise;
			return NULL;
		}

		inline WWRP<channel_future> close() {
			WAWO_ASSERT(!"TODO");
			return NULL;
		}
		inline WWRP<channel_future> close(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(!"TODO");
			(void)ch_promise;
			return NULL;
		}

		inline WWRP<channel_future> close_read() {
			WAWO_ASSERT(!"TODO");
			return NULL;
		}
		inline WWRP<channel_future> close_read(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(!"TODO");
			(void)ch_promise;
			return NULL;
		}

		inline WWRP<channel_future> close_write() {
			WAWO_ASSERT(!"TODO");
			return NULL;
		}
		inline WWRP<channel_future> close_write(WWRP<channel_promise> const& ch_promise) {
			WAWO_ASSERT(!"TODO");
			(void)ch_promise;
			return NULL;
		}

		inline void flush() {
			WAWO_ASSERT(!"TODO");
		}

	};
}}
#endif