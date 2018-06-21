#include <wawo/net/channel.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net {

	channel_handler_context::channel_handler_context(WWRP<channel> const& ch_, WWRP<channel_handler_abstract> const& h):
		P(NULL), N(NULL), m_h(h), m_io_event_loop(ch_->event_poller()), m_flag(0), ch(ch_)
	{
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
}}