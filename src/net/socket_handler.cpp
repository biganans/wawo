
#include <wawo/net/socket_handler.hpp>
#include <wawo/net/socket_pipeline.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	socket_handler_context::socket_handler_context(WWRP<socket_pipeline> const& p, WWRP<socket_handler_abstract> const& h)
		:PIPE(p), m_h(h), m_flag(0)
	{
		if (wawo::dynamic_pointer_cast<socket_activity_handler_abstract>(h) != NULL) {
			m_flag |= F_ACTIVITY;
		}

		if (wawo::dynamic_pointer_cast<socket_inbound_handler_abstract>(h) != NULL) {
			m_flag |= F_INBOUND;
		}

		if (wawo::dynamic_pointer_cast<socket_outbound_handler_abstract>(h) != NULL) {
			m_flag |= F_OUTBOUND;
		}

		if (wawo::dynamic_pointer_cast<socket_accept_handler_abstract>(h) != NULL) {
			m_flag |= F_ACCEPT;
		}
	}

	socket_handler_context::~socket_handler_context()
	{
	}

	void socket_handler_context::fire_accepted( WWRP<socket> const& so)
	{
		if (N != NULL) {
			N->invoke_accepted( so);
		}
	}

	void socket_handler_context::invoke_accepted( WWRP<socket> const& so)
	{
		if (m_flag&F_ACCEPT) {
			WWRP<socket_accept_handler_abstract> _h = wawo::dynamic_pointer_cast<socket_accept_handler_abstract>(m_h);
			_h->accepted(WWRP<socket_handler_context>(this), so);
		} else {
			fire_accepted( so);
		}
	}


#define HANDLER_CONTEXT_IMPL_H_TO_T_0(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void socket_handler_context::fire_##NAME##() \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(); \
		} \
	} \
	 \
	void socket_handler_context::invoke_##NAME##() \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<socket_handler_context>(this)); \
		} \
		else { \
			fire_##NAME##(); \
		} \
	}

	HANDLER_CONTEXT_IMPL_H_TO_T_0(connected, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(closed, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(error, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(read_shutdowned, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_shutdowned, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_block, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)
	HANDLER_CONTEXT_IMPL_H_TO_T_0(write_unblock, socket_handler_context::F_ACTIVITY, socket_activity_handler_abstract)

#define H_TO_T_HANDLER_PACKET_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void socket_handler_context::fire_##NAME##( WWSP<packet> const& p ) \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(p); \
		} \
	} \
	 \
	void socket_handler_context::invoke_##NAME##(WWSP<packet> const& p) \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<socket_handler_context>(this),p); \
		} \
		else { \
			fire_##NAME##(p); \
		} \
	}

	H_TO_T_HANDLER_PACKET_1(read, socket_handler_context::F_INBOUND, socket_inbound_handler_abstract)


#define HANDLER_CONTEXT_T_TO_H_PACKET_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void socket_handler_context::invoke_##NAME##(WWSP<packet> const& p) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<socket_handler_context>(this), p); \
		} else { \
			NAME##(p); \
		} \
	} \
	 \
	void socket_handler_context::##NAME##(WWSP<packet> const& p) { \
		WAWO_ASSERT(P != NULL); \
		P->invoke_##NAME##(p); \
	}

	HANDLER_CONTEXT_T_TO_H_PACKET_1(write, socket_handler_context::F_OUTBOUND, socket_outbound_handler_abstract)


#define HANDLER_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<socket_handler_context> const& ctx) { \
		ctx->fire_##NAME##(); \
	}

	HANDLER_IMPL_0(connected, socket_activity_handler_abstract)
	HANDLER_IMPL_0(closed, socket_activity_handler_abstract)
	HANDLER_IMPL_0(error, socket_activity_handler_abstract)
	HANDLER_IMPL_0(read_shutdowned, socket_activity_handler_abstract)
	HANDLER_IMPL_0(write_shutdowned, socket_activity_handler_abstract)
	HANDLER_IMPL_0(write_block, socket_activity_handler_abstract)
	HANDLER_IMPL_0(write_unblock, socket_activity_handler_abstract)

	void socket_handler_head::accepted(WWRP<socket_handler_context> const& ctx, WWRP<socket> const& newsocket)
	{
		ctx->fire_accepted( newsocket);
	}

	void socket_handler_head::read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income) 
	{
		ctx->fire_read(income);
	}

	void socket_handler_head::write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet)
	{
		ctx->PIPE->SO->send_packet(outlet);
	}

	//--
	void socket_handler_tail::accepted(WWRP<socket_handler_context> const& ctx, WWRP<socket> const& newsocket)
	{
		ctx->fire_accepted(newsocket);
	}

	void socket_handler_tail::read(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income)
	{
		ctx->fire_read(income);
	}

	void socket_handler_tail::write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet)
	{
		WAWO_ASSERT(!"socket_handler_head::write,,, send a flush ?");
	}

}}