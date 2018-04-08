#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/io_event.hpp>

namespace wawo { namespace net {

	class channel:
		virtual public wawo::ref_base
	{
		WWRP<channel_pipeline> m_pipeline;
		WWRP<ref_base>	m_ctx;

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

		inline WWRP<channel_pipeline> pipeline() const {
			return m_pipeline;
		}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			m_ctx = ctx;
		}

		virtual int ch_id() const = 0;
		virtual int ch_close(int const& ec) = 0;
		virtual int ch_close_read(int const& ec) = 0;
		virtual int ch_close_write(int const& ec) = 0;
		virtual int ch_write(WWSP<packet> const& outlet) = 0;

		virtual void begin_connect(WWRP<ref_base> const& cookie, fn_io_event const& fn, fn_io_event_error const& err) = 0;
		virtual void end_connect() = 0;

		virtual void begin_read(u8_t const& async_flag, WWRP<ref_base> const& cookie, fn_io_event const& fn , fn_io_event_error const& err ) = 0;
		virtual void end_read() = 0;

		virtual void begin_write(u8_t const& async_flag , WWRP<ref_base> const& cookie, fn_io_event const& fn , fn_io_event_error const& err ) = 0;
		virtual void end_write() = 0;
	};
}}
#endif