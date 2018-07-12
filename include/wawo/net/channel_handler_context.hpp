#ifndef _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP
#define _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP

#include <wawo/net/io_event_loop.hpp>

#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler.hpp>

#include <wawo/net/channel_future.hpp>

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void fire_##NAME##() { \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_next(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(); \
	} \
	inline void invoke_##NAME##() { \
		WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
		WAWO_ASSERT(m_h != NULL); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this)); \
	}

#define VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	inline void fire_##NAME##( WWRP<packet> const& p ) { \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_next(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(p); \
	} \
	inline void invoke_##NAME##(WWRP<packet> const& p) { \
		WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
		WAWO_ASSERT(m_h != NULL); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),p); \
	}

//--T_TO_H--BEGIN
#define CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
private:\
	inline void _##NAME##(WWRP<packet> const& p, WWRP<channel_promise> const& ch_promise) { \
		if( WAWO_UNLIKELY(m_flag&CH_CH_CLOSED) ) {\
			ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
			return ;\
		} \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(p,ch_promise); \
	} \
public:\
	inline WWRP<channel_future> NAME##(WWRP<packet> const& p) { \
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(ch); \
		return NAME##(p,ch_promise); \
	} \
	inline WWRP<channel_future> NAME##(WWRP<packet> const& p, WWRP<channel_promise> const& ch_promise) { \
		WAWO_ASSERT(m_io_event_loop != NULL); \
		WWRP<CTX_CLASS_NAME> _ctx(this); \
		m_io_event_loop->schedule([_ctx, p, ch_promise]() { \
			_ctx->_##NAME##(p, ch_promise); \
		}); \
		return ch_promise; \
	} \
	inline void invoke_##NAME##(WWRP<packet> const& p, WWRP<channel_promise> const& ch_promise) { \
		WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this), p, ch_promise); \
	}

#define CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
private:\
	inline void _##NAME##(WWRP<channel_promise> const& ch_promise) { \
		if( WAWO_UNLIKELY(m_flag&CH_CH_CLOSED) ) {\
			ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
			return; \
		} \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(ch_promise); \
	} \
public:\
	inline WWRP<channel_future> NAME##() { \
		WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(ch); \
		return CTX_CLASS_NAME::##NAME##(ch_promise); \
	} \
	inline WWRP<channel_future> NAME##(WWRP<channel_promise> const& ch_promise) { \
		WWRP<CTX_CLASS_NAME> _ctx(this); \
		m_io_event_loop->execute([_ctx, ch_promise]() { \
			_ctx->_##NAME##(ch_promise); \
		}); \
		return ch_promise; \
	} \
	inline void invoke_##NAME##(WWRP<channel_promise> const& ch_promise) {\
		WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),ch_promise); \
	}

#define VOID_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_0(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
private: \
	inline void _##NAME##() { \
		if( WAWO_UNLIKELY(m_flag&CH_CH_CLOSED) ) {\
			return ; \
		} \
		WWRP<CTX_CLASS_NAME> _ctx = CTX_CLASS_NAME::_find_prev(HANDLER_FLAG); \
		WAWO_ASSERT(_ctx != NULL); \
		_ctx->invoke_##NAME##(); \
	} \
public:\
	inline void NAME##() { \
		WWRP<CTX_CLASS_NAME> _ctx(this); \
		m_io_event_loop->execute([_ctx]() { \
			_ctx->_##NAME##(); \
		}); \
	} \
	inline void invoke_##NAME##() {\
		WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
		WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
		WAWO_ASSERT(_h != NULL); \
		_h->##NAME##(WWRP<CTX_CLASS_NAME>(this)); \
	}
//--T_TO_H--END

namespace wawo { namespace net {

	enum channel_handler_flag {
		CH_ACTIVITY	= 0x01,
		CH_INBOUND	= 0x02,
		CH_OUTBOUND	= 0x04,
		CH_REMOVED = 0x08,
		CH_CH_CLOSED = 0x10
	};

	class channel_handler_context :
		public ref_base,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel_pipeline;
	public:
		WWRP<channel> ch;
	private:
		WWRP<io_event_loop> m_io_event_loop;
		u16_t m_flag;
		WWRP<channel_handler_context> P;
		WWRP<channel_handler_context> N;
		WWRP<channel_handler_abstract> m_h;

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
		channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h);
		virtual ~channel_handler_context();
		inline WWRP<io_event_loop> const& event_poller() const {
			return m_io_event_loop;
		}
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, connected, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, closed, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, read_shutdowned, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_shutdowned, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_block, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_0(channel_handler_context, write_unblock, CH_ACTIVITY, channel_activity_handler_abstract)
		VOID_FIRE_HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(channel_handler_context, read, CH_INBOUND, channel_inbound_handler_abstract)

		CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_1(channel_handler_context, write, CH_OUTBOUND, channel_outbound_handler_abstract)
		CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, close, CH_OUTBOUND, channel_outbound_handler_abstract)
		CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, shutdown_read, CH_OUTBOUND, channel_outbound_handler_abstract)
		CH_FUTURE_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, shutdown_write, CH_OUTBOUND, channel_outbound_handler_abstract)

		VOID_ACTION_HANDLER_CONTEXT_IMPL_T_TO_H_0(channel_handler_context, flush, CH_OUTBOUND, channel_outbound_handler_abstract)
	};
}}
#endif