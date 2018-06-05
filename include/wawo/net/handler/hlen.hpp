#ifndef _WAWO_NET_HANDLER_HLEN_HPP
#define _WAWO_NET_HANDLER_HLEN_HPP

#include <wawo/core.hpp>
#include <wawo/net/channel_handler.hpp>

namespace wawo { namespace net { namespace handler {

	class hlen :
		public channel_inbound_handler_abstract,
		public channel_outbound_handler_abstract
	{
		enum parse_state {
			S_READ_LEN,
			S_READ_CONTENT
		};

		WAWO_DECLARE_NONCOPYABLE(hlen)

		parse_state m_state;
		u32_t m_size;
		WWRP<packet> m_tmp;

	public:
		hlen() :m_state(S_READ_LEN) {}
		virtual ~hlen() {}

		void read( WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income ) {
			WAWO_ASSERT(income != NULL);

			if (m_tmp != NULL && m_tmp->len()) {
				income->write_left(m_tmp->begin(), m_tmp->len());
				m_tmp->reset();
			}

			bool bExit = false;
			do {
				switch (m_state) {
					case S_READ_LEN:
					{
						if (income->len() < sizeof(u32_t)) {
							m_tmp = income;
							bExit = true;
							break;
						}
						m_size = income->read<u32_t>();
						m_state = S_READ_CONTENT;
					}
					break;
					case S_READ_CONTENT:
					{
						if (income->len() >= m_size) {
							WWRP<wawo::packet> _income = wawo::make_ref<wawo::packet>(m_size);
							_income->write(income->begin(), m_size);
							ctx->fire_read(_income);
							income->skip(m_size);
							m_state = S_READ_LEN;
						} else {
							m_tmp = income;
							bExit = true;
						}
					}
					break;
				}
			} while (!bExit);
		}

		void write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet,WWRP<channel_promise>& ch_promise) {
			outlet->write_left<u32_t>(outlet->len());
			ctx->write(outlet, ch_promise);
		}
	};
}}}
#endif