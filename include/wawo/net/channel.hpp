#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/io_event.hpp>
#include <wawo/net/io_executor.hpp>

namespace wawo { namespace net {

	class channel:
		public wawo::ref_base
	{
		WWRP<channel_pipeline> m_pipeline;
		WWRP<ref_base>	m_ctx;
		WWRP<io_executor> m_exector;
		int m_errno;

	public:
		channel() {}
		virtual ~channel() {
			deinit();
		}

		void init( WWRP<io_executor> const& exe ) {
			m_pipeline = wawo::make_ref<channel_pipeline>(WWRP<channel>(this));
			m_pipeline->init();
			WAWO_ALLOC_CHECK(m_pipeline, sizeof(channel_pipeline));

			WAWO_ASSERT(exe != NULL);
			m_exector = exe;
		}

		void deinit()
		{
			if (m_pipeline != NULL) {
				m_pipeline->deinit();
				m_pipeline = NULL;
			}
			m_exector = NULL;
		}

#define CH_ACTION_IMPL_PACKET_1(_NAME,_P) \
		inline void ch_##_NAME(WWRP<packet> const& _P) { \
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

		inline WWRP<channel_pipeline>& pipeline() {
			return m_pipeline;
		}
		inline WWRP<io_executor>& exector() {
			return m_exector;
		}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			m_ctx = ctx;
		}

		inline void ch_errno( int e) {
			m_errno=e;
		}
		inline int ch_get_errno() { return m_errno; }
		
		virtual int ch_id() const = 0;
		virtual int ch_close() = 0;
		virtual int ch_close_read() = 0;
		virtual int ch_close_write() = 0;
		virtual int ch_write(WWRP<packet> const& outlet) = 0;

		/*
		virtual void begin_connect(WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_connected = NULL, fn_io_event_error const& fn_err = NULL) {
			(void)cookie;
			(void)fn_connected;
			(void)fn_err;
		}
		virtual void end_connect() {}
		*/
		virtual void begin_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
			(void)async_flag;
			(void)cookie;
			(void)fn_read;
			(void)fn_err;
		}
		virtual void end_read() {}

		virtual void begin_write(u8_t const& async_flag =0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL) {
			(void)async_flag;
			(void)cookie;
			(void)fn_write;
			(void)fn_err;
		}
		virtual void end_write() {}

		virtual int turnon_nodelay() { return wawo::OK; }
		virtual int turnoff_nodelay() { return wawo::OK; }

		virtual bool is_active() const = 0;
	};
}}
#endif