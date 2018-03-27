#ifndef _WAWO_NET_HANDLER_HLEN_HPP
#define _WAWO_NET_HANDLER_HLEN_HPP

#include <wawo/core.hpp>
#include <wawo/net/socket_handler.hpp>

namespace wawo { namespace net { namespace handler {

	class hlen_handler :
		public socket_inbound_handler_abstract,
		public socket_outbound_handler_abstract
	{
		enum parse_state {
			S_READ_LEN,
			S_READ_CONTENT
		};

		WAWO_DECLARE_NONCOPYABLE(hlen_handler)

		parse_state m_state;
		u32_t m_size;
		WWSP<packet> m_tmp;

	public:
		hlen_handler() :m_state(S_READ_LEN) {}
		virtual ~hlen_handler() {}

		void read( WWRP<socket_handler_context> const& ctx, WWSP<packet> const& income ) {
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
						m_tmp = wawo::make_shared<packet>(m_size);
					}
					break;
					case S_READ_CONTENT:
					{
						if (m_size == 0) {
							WAWO_ASSERT(m_tmp != NULL);
							ctx->fire_read(m_tmp);
							m_state = S_READ_LEN;
						} else {
							u32_t nbytes = income->read(m_tmp->begin(), m_size );
							WAWO_ASSERT(nbytes <= m_size);
							m_tmp->forward_write_index(nbytes);
							m_size -= nbytes;

							if (income->len() == 0) {
								bExit = true;
							}
						}
					}
					break;
				}

			} while (!bExit);
		}

		void write(WWRP<socket_handler_context> const& ctx, WWSP<packet> const& outlet) {
			outlet->write_left<u32_t>(outlet->len());
			ctx->write(outlet);
		}
	};
}}}
#endif