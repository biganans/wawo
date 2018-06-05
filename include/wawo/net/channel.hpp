#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/io_event.hpp>
#include <wawo/net/io_event_loop.hpp>

namespace wawo { namespace net {

	class channel:
		public wawo::ref_base
	{
		WWRP<channel_pipeline> m_pipeline;
		WWRP<ref_base>	m_ctx;
		WWRP<io_event_loop> m_io_event_loop;
		int m_errno;
		WWRP<channel_promise> m_ch_close_future;

	public:
		channel():
			m_pipeline(NULL),
			m_ctx(NULL),
			m_io_event_loop(NULL),
			m_errno(0),
			m_ch_close_future(NULL)
		{}
		virtual ~channel() {
			deinit();
		}

		void init( WWRP<io_event_loop> const& loop ) {
			m_pipeline = wawo::make_ref<channel_pipeline>(WWRP<channel>(this));
			m_pipeline->init();
			WAWO_ALLOC_CHECK(m_pipeline, sizeof(channel_pipeline));

			WAWO_ASSERT(loop != NULL);
			m_io_event_loop = loop;

			WAWO_ASSERT(m_ch_close_future == NULL);
			m_ch_close_future = wawo::make_ref<channel_promise>();
		}

		void deinit()
		{
			if (m_pipeline != NULL) {
				m_pipeline->deinit();
				m_pipeline = NULL;
			}
			m_io_event_loop = NULL;
		}

		inline WWRP<io_event_loop> event_loop() const {
			return m_io_event_loop;
		}

		inline WWRP<channel_pipeline>& pipeline() {
			return m_pipeline;
		}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}

		inline void set_ctx(WWRP<ref_base> const& ctx) {
			m_ctx = ctx;
		}

		inline void ch_errno(int e) {
			m_errno = e;
		}
		inline int ch_get_errno() { return m_errno; }

		inline WWRP<channel_future> ch_close_future() const {
			return m_ch_close_future;
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

	
		//could be called directly in any place
		virtual int ch_id() const = 0;
		virtual void ch_close(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_close_read(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_close_write(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_write(WWRP<packet> const& outlet, WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_flush() = 0;

		//called by context in event_loop
		virtual void ch_close_impl(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_close_read_impl(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_close_write_impl(WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_write_imple(WWRP<packet> const& outlet, WWRP<channel_promise>& ch_promise) = 0;
		virtual void ch_flush_impl() = 0;

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
		virtual bool is_read_shutdowned() const = 0;
		virtual bool is_write_shutdowned() const = 0;
		virtual bool is_readwrite_shutdowned() const = 0;
		virtual bool is_closed() const = 0;
	};
}}
#endif