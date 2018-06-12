#include <wawo/net/channel_future.hpp>
#include <wawo/net/channel.hpp>

namespace wawo { namespace net {
	channel_future::channel_future(WWRP<wawo::net::channel> const& ch) :
		m_ch(ch)
	{}
	channel_future::~channel_future()
	{}
	WWRP<wawo::net::channel> channel_future::channel() { return m_ch; }
	void channel_future::reset() { m_ch = NULL; }
}}