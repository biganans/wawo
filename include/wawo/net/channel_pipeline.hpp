#ifndef _WAWO_NET_CHANNEL_PIPELINE_HPP
#define _WAWO_NET_CHANNEL_PIPELINE_HPP


#include <wawo/core.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler_context.hpp>
#include <wawo/net/io_event_loop.hpp>

#define PIPELINE_VOID_FIRE_VOID(NAME) \
	inline void fire_##NAME() {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_head != NULL); \
		m_io_event_loop->execute([h=m_head](){ \
			h->fire_##NAME(); \
		}); \
	}\

#define PIPELINE_VOID_FIRE_PACKET_1(NAME) \
	inline void fire_##NAME(WWRP<packet> const& packet_) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_head != NULL); \
		m_io_event_loop->execute([h=m_head,packet_](){ \
			h->fire_##NAME(packet_); \
		}); \
	}\

#define PIPELINE_CH_FUTURE_ACTION_PACKET_1(NAME) \
	inline WWRP<channel_future> NAME(WWRP<packet> const& packet_) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_tail != NULL); \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(m_ch); \
		m_io_event_loop->execute([t=m_tail,packet_,ch_promise](){ \
			t->##NAME(packet_,ch_promise); \
		}); \
		return ch_promise; \
	}\

#define PIPELINE_CH_FUTURE_ACTION_PACKET_1_CH_PROMISE_1(NAME) \
	inline WWRP<channel_future> NAME(WWRP<packet> const& packet_, WWRP<channel_promise> const& ch_promise) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_tail != NULL); \
		m_io_event_loop->execute([t=m_tail,packet_,ch_promise](){ \
			t->##NAME(packet_,ch_promise); \
		}); \
		return ch_promise; \
	}\

#define PIPELINE_CH_FUTURE_ACTION_VOID(NAME) \
	inline WWRP<channel_future> NAME() {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_tail != NULL); \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(m_ch); \
		m_io_event_loop->execute([t=m_tail,ch_promise](){ \
			t->##NAME(ch_promise); \
		}); \
		return ch_promise; \
	}\

#define PIPELINE_CH_FUTURE_ACTION_CH_PROMISE_1(NAME) \
	inline WWRP<channel_future> NAME( WWRP<channel_promise> const& ch_promise) {\
		WAWO_ASSERT(m_tail != NULL); \
		WAWO_ASSERT(m_io_event_loop != NULL); \
		m_io_event_loop->execute([t=m_tail,ch_promise](){ \
			t->##NAME(ch_promise); \
		}); \
		return ch_promise; \
	}\

#define PIPELINE_VOID_ACTION_VOID(NAME) \
	inline void NAME() {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WAWO_ASSERT(m_tail != NULL); \
		m_io_event_loop->execute([t=m_tail](){ \
			t->##NAME(); \
		}); \
		return ; \
	}\

namespace wawo { namespace net {

	//class channel;
	class channel_pipeline :
		public ref_base,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel;

	private:
		WWRP<channel> m_ch;
		WWRP<io_event_loop> m_io_event_loop;
		WWRP<channel_handler_context> m_head;
		WWRP<channel_handler_context> m_tail;

		inline void _add_last(WWRP<channel_handler_abstract> const& h) {
			WAWO_ASSERT(m_ch != NULL);
			WWRP<channel_handler_context> ctx = wawo::make_ref<channel_handler_context>(m_ch, h);
			ctx->N = m_tail;
			ctx->P = m_tail->P;

			m_tail->P->N = ctx;
			m_tail->P = ctx;
		}
	public:
		channel_pipeline(WWRP<channel> const& ch);
		virtual ~channel_pipeline();

		void init();
		void deinit();
		inline void add_last(WWRP<channel_handler_abstract> const& h) {
			m_io_event_loop->execute([P = WWRP<channel_pipeline>(this), h]() -> void {
				P->_add_last(h);
			});
		}

	protected:
		PIPELINE_VOID_FIRE_VOID(connected)
		PIPELINE_VOID_FIRE_VOID(closed)
		PIPELINE_VOID_FIRE_VOID(read_shutdowned)
		PIPELINE_VOID_FIRE_VOID(write_shutdowned)
		PIPELINE_VOID_FIRE_VOID(write_block)
		PIPELINE_VOID_FIRE_VOID(write_unblock)
		PIPELINE_VOID_FIRE_PACKET_1(read)

		PIPELINE_CH_FUTURE_ACTION_PACKET_1(write)
		PIPELINE_CH_FUTURE_ACTION_PACKET_1_CH_PROMISE_1(write)

		PIPELINE_CH_FUTURE_ACTION_VOID(close)
		PIPELINE_CH_FUTURE_ACTION_VOID(shutdown_read)
		PIPELINE_CH_FUTURE_ACTION_VOID(shutdown_write)

		PIPELINE_CH_FUTURE_ACTION_CH_PROMISE_1(close)
		PIPELINE_CH_FUTURE_ACTION_CH_PROMISE_1(shutdown_read)
		PIPELINE_CH_FUTURE_ACTION_CH_PROMISE_1(shutdown_write)

		PIPELINE_VOID_ACTION_VOID(flush)
	};
}}
#endif