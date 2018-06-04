#include <wawo/net/channel.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net {

	channel_handler_context::channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h)
		:m_h(h), m_flag(0),P(NULL),N(NULL),ch(ch_)
	{
		if (wawo::dynamic_pointer_cast<channel_acceptor_handler_abstract>(h) != NULL) {
			m_flag |= CH_ACCEPTOR;
		}

		if (wawo::dynamic_pointer_cast<channel_activity_handler_abstract>(h) != NULL) {
			m_flag |= CH_ACTIVITY;
		}

		if (wawo::dynamic_pointer_cast<channel_inbound_handler_abstract>(h) != NULL) {
			m_flag |= CH_INBOUND;
		}

		if (wawo::dynamic_pointer_cast<channel_outbound_handler_abstract>(h) != NULL) {
			m_flag |= CH_OUTBOUND;
		}
	}

	channel_handler_context::~channel_handler_context()
	{
	}

	HANDLER_CONTEXT_IMPL_H_TO_T_CHANNEL_1(channel_handler_context, channel, accepted, channel_acceptor_handler_abstract)

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
	CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, close_read, CH_OUTBOUND, channel_outbound_handler_abstract)
	CH_FUTURE_HANDLER_CONTEXT_IMPL_T_TO_H_PROMISE(channel_handler_context, close_write, CH_OUTBOUND, channel_outbound_handler_abstract)

	/*
	void channel_handler_context::begin_connect(WWRP<ref_base> const& cookie, fn_io_event const& fn_connected , fn_io_event_error const& fn_err ) {
		WAWO_ASSERT(ch != NULL);
		ch->begin_connect(cookie, fn_connected, fn_err);
	}

	void channel_handler_context::end_connect() {
		WAWO_ASSERT(ch != NULL);
		ch->end_connect();
	}
	*/
	void channel_handler_context::begin_read(u8_t const& async_flag , WWRP<ref_base> const& cookie, fn_io_event const& fn_read , fn_io_event_error const& fn_err ) {
		WAWO_ASSERT(ch != NULL);
		ch->begin_read(async_flag, cookie, fn_read, fn_err);
	}

	void channel_handler_context::end_read() {
		WAWO_ASSERT(ch != NULL);
		ch->end_read();
	}

	void channel_handler_context::begin_write(u8_t const& async_flag , WWRP<ref_base> const& cookie, fn_io_event const& fn_write, fn_io_event_error const& fn_err ) {
		WAWO_ASSERT(ch != NULL);
		ch->begin_write(async_flag, cookie, fn_write, fn_err);
	}
	void channel_handler_context::end_write() {
		WAWO_ASSERT(ch != NULL);
		ch->end_write();
	}

}}