#ifndef _WAWO_NET_CHANNEL_FUTURE_HPP
#define _WAWO_NET_CHANNEL_FUTURE_HPP

#include <wawo/future.hpp>

namespace wawo { namespace net {

	class channel;
	class channel_future:
		public wawo::future<int>
	{
		WWRP<wawo::net::channel> m_ch;
	public:
		channel_future(WWRP<wawo::net::channel> const& ch);
		virtual ~channel_future();
		WWRP<wawo::net::channel> channel();

		void reset();
	};

	class channel_promise:
		public channel_future
	{
	public:
		channel_promise(WWRP<wawo::net::channel> const& ch):
			channel_future(ch)
		{}

		void set_success(int const& v) {
			lock_guard<mutex> lg(channel_future::m_mutex);
			typename channel_future::state s = channel_future::S_IDLE;
			int ok = channel_future::m_state.compare_exchange_strong(s, channel_future::S_SUCCESS, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);
			int i = int();
			ok = channel_future::m_v.compare_exchange_strong(i, v, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);
			(void)ok;

			channel_future::_notify_waiter();
			channel_future::_notify_listeners();
		}

		void set_failure(WWSP<wawo::promise_exception> const& e) {
			lock_guard<mutex> lg(channel_future::m_mutex);
			typename channel_future::state s = channel_future::S_IDLE;
			int ok = channel_future::m_state.compare_exchange_strong(s, channel_future::S_FAILURE, std::memory_order_acq_rel);
			WAWO_ASSERT(ok);
			(void)ok;

			channel_future::m_exception = e;

			channel_future::_notify_waiter();
			channel_future::_notify_listeners();
		}
	};

}}

#endif