#include <wawo/net/channel.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net {


	channel_handler_context::channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h):
		P(NULL), N(NULL), m_h(h), m_io_event_loop(ch_->event_loop()), m_flag(0), ch(ch_)
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

	channel_handler_context::~channel_handler_context() {}


	/*
	inline void channel_handler_context::begin_read(u8_t const& async_flag = 0, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
		WAWO_ASSERT(ch != NULL);
		ch->begin_read(async_flag, fn_read, fn_err);
	}
	inline void channel_handler_context::end_read() {
		WAWO_ASSERT(ch != NULL);
		ch->end_read();
	}
	inline void channel_handler_context::begin_write(u8_t const& async_flag = 0, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL) {
		WAWO_ASSERT(ch != NULL);
		ch->begin_write(async_flag, fn_write, fn_err);
	}
	inline void channel_handler_context::end_write() {
		WAWO_ASSERT(ch != NULL);
		ch->end_write();
	}
	*/
}}