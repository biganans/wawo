
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/channel.hpp>

namespace wawo { namespace net {

	channel_handler_context::channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h)
		:ch(ch_), m_h(h), m_flag(0)
	{
		if (wawo::dynamic_pointer_cast<channel_activity_handler_abstract>(h) != NULL) {
			m_flag |= F_ACTIVITY;
		}

		if (wawo::dynamic_pointer_cast<channel_inbound_handler_abstract>(h) != NULL) {
			m_flag |= F_INBOUND;
		}

		if (wawo::dynamic_pointer_cast<channel_outbound_handler_abstract>(h) != NULL) {
			m_flag |= F_OUTBOUND;
		}

		if (wawo::dynamic_pointer_cast<channel_accept_handler_abstract>(h) != NULL) {
			m_flag |= F_ACCEPT;
		}
	}

	channel_handler_context::~channel_handler_context()
	{
	}

	void channel_handler_context::fire_accepted( WWRP<channel> const& so_ )
	{
		if (N != NULL) {
			N->invoke_accepted(so_);
		}
	}

	void channel_handler_context::invoke_accepted( WWRP<channel> const& ch_)
	{
		if (m_flag&F_ACCEPT) {
			WWRP<channel_accept_handler_abstract> _h = wawo::dynamic_pointer_cast<channel_accept_handler_abstract>(m_h);
			_h->accepted(WWRP<channel_handler_context>(this), ch_);
		} else {
			fire_accepted(ch_);
		}
	}


#define HANDLER_CONTEXT_IMPL_H_TO_T_0(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void channel_handler_context::fire_##NAME##() \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(); \
		} \
	} \
	 \
	void channel_handler_context::invoke_##NAME##() \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<channel_handler_context>(this)); \
		} \
		else { \
			fire_##NAME##(); \
		} \
	}

	HANDLER_CONTEXT_IMPL_H_TO_T_0(connected, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(closed, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(error, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(read_shutdowned, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_shutdowned, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_block, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_unblock, channel_handler_context::F_ACTIVITY, channel_activity_handler_abstract)

#define H_TO_T_HANDLER_PACKET_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void channel_handler_context::fire_##NAME##( WWSP<packet> const& p ) \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(p); \
		} \
	} \
	 \
	void channel_handler_context::invoke_##NAME##(WWSP<packet> const& p) \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<channel_handler_context>(this),p); \
		} \
		else { \
			fire_##NAME##(p); \
		} \
	}

	H_TO_T_HANDLER_PACKET_1(read, channel_handler_context::F_INBOUND, channel_inbound_handler_abstract)

#define INT_HANDLER_CONTEXT_T_TO_H_PACKET_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	int channel_handler_context::invoke_##NAME##(WWSP<packet> const& p) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			return _h->##NAME##(WWRP<channel_handler_context>(this), p); \
		} else { \
			return NAME##(p); \
		} \
	} \
	 \
	int channel_handler_context::##NAME##(WWSP<packet> const& p) { \
		WAWO_ASSERT(P != NULL); \
		return P->invoke_##NAME##(p); \
	}

	INT_HANDLER_CONTEXT_T_TO_H_PACKET_1(write, channel_handler_context::F_OUTBOUND, channel_outbound_handler_abstract)

#define INT_HANDLER_CONTEXT_T_TO_H_INT_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	int channel_handler_context::invoke_##NAME##(int const& i) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			return _h->##NAME##(WWRP<channel_handler_context>(this), i); \
		} else { \
			return NAME##(i); \
		} \
	} \
	 \
	int channel_handler_context::##NAME##(int const& i) { \
		WAWO_ASSERT(P != NULL); \
		return P->invoke_##NAME##(i); \
	}

	INT_HANDLER_CONTEXT_T_TO_H_INT_1(close, channel_handler_context::F_OUTBOUND, channel_outbound_handler_abstract)
	INT_HANDLER_CONTEXT_T_TO_H_INT_1(close_read, channel_handler_context::F_OUTBOUND, channel_outbound_handler_abstract)
	INT_HANDLER_CONTEXT_T_TO_H_INT_1(close_write, channel_handler_context::F_OUTBOUND, channel_outbound_handler_abstract)


#define HANDLER_FIRE_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx) { \
		ctx->fire_##NAME##(); \
	}

	HANDLER_FIRE_IMPL_0(connected, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(closed, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(error, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(read_shutdowned, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_shutdowned, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_block, channel_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_unblock, channel_activity_handler_abstract)

#define INT_HANDLER_IMPL_INT_1(NAME,HANDLER_NAME) \
	int HANDLER_NAME##::##NAME##(WWRP<channel_handler_context> const& ctx, int const& code) { \
		return ctx->##NAME##(code); \
	}

	INT_HANDLER_IMPL_INT_1(close, channel_outbound_handler_abstract)
	INT_HANDLER_IMPL_INT_1(close_read, channel_outbound_handler_abstract)
	INT_HANDLER_IMPL_INT_1(close_write, channel_outbound_handler_abstract)


	void channel_handler_head::accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch)
	{
		ctx->fire_accepted( newch );
	}

	void channel_handler_head::read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income) 
	{
		ctx->fire_read(income);
	}

	int channel_handler_head::write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet)
	{
		return ctx->ch->ch_write(outlet);
	}

	int channel_handler_head::close(WWRP<channel_handler_context> const& ctx, int const& code) {
		return ctx->ch->ch_close(code);
	}

	int channel_handler_head::close_read(WWRP<channel_handler_context> const& ctx, int const& code) {
		return ctx->ch->ch_close_read(code);
	}

	int channel_handler_head::close_write(WWRP<channel_handler_context> const& ctx, int const& code) {
		return ctx->ch->ch_close_write(code);
	}

	//--
	void socket_handler_tail::accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch )
	{
		ctx->fire_accepted(newch);
	}

	void socket_handler_tail::read(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& income)
	{
		ctx->fire_read(income);
	}

	int socket_handler_tail::write(WWRP<channel_handler_context> const& ctx, WWSP<packet> const& outlet)
	{
		WAWO_ASSERT(!"socket_handler_head::write,,, send a flush ?");
		return wawo::OK;

		(void)ctx;
		(void)outlet;
	}

}}