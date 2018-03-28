
#include <wawo/net/socket_handler.hpp>
#include <wawo/net/socket_pipeline.hpp>
#include <wawo/net/socket.hpp>

namespace wawo { namespace net {

	socket_handler_context::socket_handler_context(WWRP<socket> const& so_, WWRP<socket_handler_abstract> const& h)
		:so(so_), m_h(h), m_flag(0)
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

	void socket_handler_context::fire_accepted( WWRP<socket> const& so_ )
	{
		if (N != NULL) {
			N->invoke_accepted(so_);
		}
	}

	void socket_handler_context::invoke_accepted( WWRP<socket> const& so_)
	{
		if (m_flag&F_ACCEPT) {
			WWRP<socket_accept_handler_abstract> _h = wawo::dynamic_pointer_cast<socket_accept_handler_abstract>(m_h);
			_h->accepted(WWRP<socket_handler_context>(this), so_);
		} else {
			fire_accepted(so_);
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

#define HANDLER_CONTEXT_T_TO_H_INT_1(NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void socket_handler_context::invoke_##NAME##(int const& i) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<socket_handler_context>(this), i); \
		} else { \
			NAME##(i); \
		} \
	} \
	 \
	void socket_handler_context::##NAME##(int const& i) { \
		WAWO_ASSERT(P != NULL); \
		P->invoke_##NAME##(i); \
	}

	HANDLER_CONTEXT_T_TO_H_INT_1(close, socket_handler_context::F_OUTBOUND, socket_outbound_handler_abstract)
	HANDLER_CONTEXT_T_TO_H_INT_1(close_read, socket_handler_context::F_OUTBOUND, socket_outbound_handler_abstract)
	HANDLER_CONTEXT_T_TO_H_INT_1(close_write, socket_handler_context::F_OUTBOUND, socket_outbound_handler_abstract)


#define HANDLER_FIRE_IMPL_0(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<socket_handler_context> const& ctx) { \
		ctx->fire_##NAME##(); \
	}

	HANDLER_FIRE_IMPL_0(connected, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(closed, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(error, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(read_shutdowned, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_shutdowned, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_block, socket_activity_handler_abstract)
	HANDLER_FIRE_IMPL_0(write_unblock, socket_activity_handler_abstract)

#define HANDLER_IMPL_INT_1(NAME,HANDLER_NAME) \
	void HANDLER_NAME##::##NAME##(WWRP<socket_handler_context> const& ctx, int const& code) { \
		ctx->##NAME##(code); \
	}

	HANDLER_IMPL_INT_1(close, socket_outbound_handler_abstract)
	HANDLER_IMPL_INT_1(close_read, socket_outbound_handler_abstract)
	HANDLER_IMPL_INT_1(close_write, socket_outbound_handler_abstract)


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
		ctx->so->send_packet(outlet);
	}

	void socket_handler_head::close(WWRP<socket_handler_context> const& ctx, int const& code) {
		ctx->so->close(code);
	}

	void socket_handler_head::close_read(WWRP<socket_handler_context> const& ctx, int const& code) {
		ctx->so->shutdown(SHUTDOWN_RD, code);
	}

	void socket_handler_head::close_write(WWRP<socket_handler_context> const& ctx, int const& code) {
		ctx->so->shutdown(SHUTDOWN_WR,code);
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