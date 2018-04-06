#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/channel_pipeline.hpp>

namespace wawo { namespace net {

	class channel :
		virtual public wawo::ref_base
	{
		WWRP<channel_pipeline> m_pipeline;

	public:
		channel() {}
		virtual ~channel() {}

		void init() {
			m_pipeline = wawo::make_ref<channel_pipeline>(WWRP<channel>(this));
			m_pipeline->init();
			WAWO_ALLOC_CHECK(m_pipeline, sizeof(channel_pipeline));
		}

		void deinit()
		{
			if (m_pipeline != NULL) {
				m_pipeline->deinit();
				m_pipeline = NULL;
			}
		}
		inline WWRP<channel_pipeline> pipeline() const {
			return m_pipeline;
		}

		virtual int ch_id() const = 0;

		virtual int ch_close(int const& ec) = 0;
		virtual int ch_close_read(int const& ec) = 0;
		virtual int ch_close_write(int const& ec) = 0;
		virtual int ch_write(WWSP<packet> const& outlet) = 0;


#define CH_ACTION_IMPL_PACKET_1(_NAME,_P) \
		inline void ch_##_NAME(WWSP<packet> const& _P) { \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_pipeline->fire_##_NAME(_P); \
		} \

		CH_ACTION_IMPL_PACKET_1(read,income)

#define CH_ACTION_IMPL_CHANNEL_1(_NAME,_CH) \
		inline void ch_##_NAME(WWRP<channel> const& _CH) { \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_pipeline->fire_##_NAME(_CH); \
		} \

		CH_ACTION_IMPL_CHANNEL_1(accepted,ch)

#define CH_ACTION_IMPL_0(_NAME) \
		inline void ch_##_NAME() { \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_pipeline->fire_##_NAME(); \
		} \

		CH_ACTION_IMPL_0(connected)
		CH_ACTION_IMPL_0(closed)
		CH_ACTION_IMPL_0(error)
		CH_ACTION_IMPL_0(read_shutdowned)
		CH_ACTION_IMPL_0(write_shutdowned)

		CH_ACTION_IMPL_0(write_block)
		CH_ACTION_IMPL_0(write_unblock)
	};
}}
#endif