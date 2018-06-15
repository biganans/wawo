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
		if(!m_io_event_loop->in_event_loop()) {\
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p](){ \
				_p->fire_##NAME(); \
			}); \
			return ; \
		}\
		m_head->fire_##NAME(); \
	}\

#define PIPELINE_VOID_FIRE_PACKET_1(NAME) \
	inline void fire_##NAME(WWRP<packet> const& packet_) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		if(!m_io_event_loop->in_event_loop()) {\
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p,packet_](){ \
				_p->fire_##NAME(packet_); \
			}); \
			return ; \
		}\
		m_head->fire_##NAME(packet_); \
	}\

#define PIPELINE_CH_FUTURE_ACTION_PACKET_1(NAME) \
	inline WWRP<channel_future> NAME(WWRP<packet> const& packet_) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(m_ch); \
		if(!m_io_event_loop->in_event_loop()) { \
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p,packet_,ch_promise](){ \
				_p->##NAME(packet_,ch_promise); \
			}); \
			return ch_promise; \
		}\
		return m_tail->##NAME(packet_,ch_promise); \
	}\

#define PIPELINE_CH_FUTURE_ACTION_PACKET_1_CH_PROMISE_1(NAME) \
	inline WWRP<channel_future> NAME(WWRP<packet> const& packet_, WWRP<channel_promise> const& ch_promise) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		if(!m_io_event_loop->in_event_loop()) { \
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p,packet_,ch_promise](){ \
				_p->##NAME(packet_,ch_promise); \
			}); \
			return ch_promise; \
		}\
		return m_tail->##NAME(packet_,ch_promise); \
	}\

#define PIPELINE_CH_FUTURE_ACTION_VOID(NAME) \
	inline WWRP<channel_future> NAME() {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(m_ch); \
		if(!m_io_event_loop->in_event_loop()) { \
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p,ch_promise](){ \
				_p->##NAME(ch_promise); \
			}); \
			return ch_promise; \
		}\
		return m_tail->##NAME(ch_promise); \
	}\

#define PIPELINE_CH_FUTURE_ACTION_CH_PROMISE_1(NAME) \
	inline WWRP<channel_future> NAME( WWRP<channel_promise> const& ch_promise) {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		if(!m_io_event_loop->in_event_loop()) { \
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p,ch_promise](){ \
				_p->##NAME(ch_promise); \
			}); \
			return ch_promise; \
		}\
		return m_tail->##NAME(ch_promise); \
	}\

#define PIPELINE_VOID_ACTION_VOID(NAME) \
	inline void NAME() {\
		WAWO_ASSERT(m_io_event_loop != NULL); \
		if(!m_io_event_loop->in_event_loop()) { \
			WWRP<channel_pipeline> _p(this); \
			m_io_event_loop->schedule([_p](){ \
				_p->##NAME(); \
			}); \
			return ; \
		}\
		m_tail->##NAME(); \
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

	public:
		channel_pipeline(WWRP<channel> const& ch);
		virtual ~channel_pipeline();

		void init();
		void deinit();
		WWRP<channel_pipeline> add_last(WWRP<channel_handler_abstract> const& h);

	protected:
		PIPELINE_VOID_FIRE_VOID(connected)
		PIPELINE_VOID_FIRE_VOID(closed)
		PIPELINE_VOID_FIRE_VOID(error)
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