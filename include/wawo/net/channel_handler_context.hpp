#ifndef _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP
#define _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP

#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/io_event.hpp>

#define HANDLER_CONTEXT_IMPL_H_TO_T_CHANNEL_1(CTX_CLASS_NAME,CHANNEL_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void fire_##NAME##( WWRP<CHANNEL_NAME> const& ch_ ) \
	{ \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_next(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(ch_); \
	} \
	inline void invoke_##NAME##( WWRP<CHANNEL_NAME> const& ch_) \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),ch_); \
	}

#define HANDLER_CONTEXT_IMPL_H_TO_T_0(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void fire_##NAME##() \
	{ \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_next(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(); \
	} \
	 \
	inline void invoke_##NAME##() \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this)); \
	}

#define HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void fire_##NAME##( WWRP<packet> const& p ) \
	{ \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_next(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(p); \
	} \
	 \
	inline void invoke_##NAME##(WWRP<packet> const& p) \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),p); \
	}

//--T_TO_H--BEGIN
#define CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline WWRP<channel_future> NAME##(WWRP<packet> const& p) { \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(); \
		return NAME##(p,ch_promise); \
	} \
	inline WWRP<channel_future> NAME##(WWRP<packet> const& p, WWRP<channel_promise> const& ch_promise) { \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(p,ch_promise); \
		return ch_promise; \
	} \
	void CTX_CLASS_NAME::invoke_##NAME##(WWRP<packet> const& p, WWRP<channel_promise> const& ch_promise) { \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this), p, ch_promise); \
	}

#define CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline WWRP<channel_future> NAME##() { \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(); \
		return CTX_CLASS_NAME::##NAME##(ch_promise); \
	} \
	inline WWRP<channel_future> NAME##(WWRP<channel_promise> const& ch_promise) { \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(ch_promise); \
		return ch_promise; \
	} \
	inline void invoke_##NAME##(WWRP<channel_promise> const& ch_promise) {\
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),ch_promise); \
	}

#define VOID_HANDLER_CONTEXT_IMPL_T_TO_H_0(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void NAME##() { \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(); \
	} \
	void CTX_CLASS_NAME::invoke_##NAME##() {\
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this)); \
	}
//--T_TO_H--END

namespace wawo { namespace net {

	enum channel_handler_flag {
		CH_ACCEPTOR	= 0x01,
		CH_ACTIVITY	= 0x02,
		CH_INBOUND	= 0x04,
		CH_OUTBOUND	= 0x08,
	};

	class channel_handler_context :
		public ref_base,
		public channel_acceptor_invoker_abstract,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel_pipeline;

		WWRP<channel_handler_context> P;
		WWRP<channel_handler_context> N;
		WWRP<channel_handler_abstract> m_h;

		u8_t m_flag;

		inline WWRP<channel_handler_context> _find_next(channel_handler_flag const& f) {
			WWRP<channel_handler_context> ctx = WWRP<channel_handler_context>(this);
			do {
				ctx = ctx->N;
				WAWO_ASSERT(ctx != NULL);
			}while (!(ctx->m_flag&f));
			return ctx;
		}

		inline WWRP<channel_handler_context> _find_prev(channel_handler_flag const& f) {
			WWRP<channel_handler_context> ctx = WWRP<channel_handler_context>(this);
			do {
				ctx = ctx->P;
				WAWO_ASSERT(ctx != NULL);
			} while (!(ctx->m_flag&f));
			return ctx;
		}

	public:
		WWRP<channel> ch;

		channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h);
		virtual ~channel_handler_context();

		HANDLER_CONTEXT_IMPL_H_TO_T_CHANNEL_1(channel_handler_context, channel, accepted, CH_ACCEPTOR, channel_acceptor_handler_abstract)

		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, connected, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, closed, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, error, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, read_shutdowned, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_shutdowned, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_block, CH_ACTIVITY, channel_activity_handler_abstract)
		HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_unblock, CH_ACTIVITY, channel_activity_handler_abstract)

		HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(channel_handler_context, read, CH_INBOUND, channel_inbound_handler_abstract)


		CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_1(channel_handler_context, write, CH_OUTBOUND, channel_outbound_handler_abstract)

		CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, close, CH_OUTBOUND, channel_outbound_handler_abstract)
		CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, shutdown_read, CH_OUTBOUND, channel_outbound_handler_abstract)
		CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, shutdown_write, CH_OUTBOUND, channel_outbound_handler_abstract)

		VOID_HANDLER_CONTEXT_IMPL_T_TO_H_0(channel_handler_context, flush, CH_OUTBOUND, channel_outbound_handler_abstract)

		/*
		inline void begin_read(u8_t const& async_flag = 0, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
			WAWO_ASSERT(ch != NULL);
			ch->begin_read(async_flag, fn_read, fn_err);
		}
		inline void end_read() {
			WAWO_ASSERT(ch != NULL);
			ch->end_read();
		}
		inline void begin_write(u8_t const& async_flag = 0, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL) {
			WAWO_ASSERT(ch != NULL);
			ch->begin_write(async_flag, fn_write, fn_err);
		}
		inline void end_write() {
			WAWO_ASSERT(ch != NULL);
			ch->end_write();
		}
		*/
	};
}}
#endif