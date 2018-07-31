#include <wawo/net/channel_future.hpp>
#include <wawo/net/channel.hpp>

namespace wawo { namespace net {
	channel_future::channel_future(WWRP<wawo::net::channel> const& ch) :
		m_ch(ch)
	{}
	channel_future::~channel_future()
	{}
	WWRP<wawo::net::channel> const& channel_future::channel() const { return m_ch; }
	void channel_future::attach_channel(WWRP<wawo::net::channel> const& ch) {
		WAWO_ASSERT(m_ch == NULL);
		m_ch = ch;
	}
	void channel_future::reset() { m_ch = NULL; }
}}