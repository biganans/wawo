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

			bool bExit = false;
			do {
				switch (m_state) {
					case S_READ_LEN:
					{
						if (income->len() < sizeof(u32_t)) {
							bExit = true;
							break;
						}
						m_size = income->read<u32_t>();
						m_state = S_READ_CONTENT;
						m_tmp = wawo::make_ref<packet>(m_size);
					}
					break;
					case S_READ_CONTENT:
					{
						if (m_size == 0) {
							WAWO_ASSERT(m_tmp != NULL);
							ctx->fire_read(m_tmp);
							m_state = S_READ_LEN;
						} else {
							if (income->len() == 0) {
								bExit = true;
							}
							u32_t nbytes = income->read(m_tmp->begin(), m_size );
							WAWO_ASSERT(nbytes <= m_size);
							m_tmp->forward_write_index(nbytes);
							m_size -= nbytes;
						}
					}
					break;
				}

			} while (!bExit);
		}

		int write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet) {
			outlet->write_left<u32_t>(outlet->len());
			return ctx->write(outlet);
		}
	};
}}}
#endif