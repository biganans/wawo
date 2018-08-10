#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/io_event_loop.hpp>

#include <wawo/net/channel_future.hpp>
#include <wawo/net/channel_pipeline.hpp>

namespace wawo { namespace net {

	enum channel_flag {
		F_NONE = 0,
		F_READ_SHUTDOWN = 1,
		F_WRITE_SHUTDOWN = 1 << 1,
		F_READWRITE_SHUTDOWN = (F_READ_SHUTDOWN | F_WRITE_SHUTDOWN),

		F_WATCH_READ = 1 << 2,
		F_WATCH_READ_INFINITE = 1 << 3,
		F_WATCH_WRITE = 1 << 4,

		F_WRITE_BLOCKED = 1 << 5,
		F_WRITE_SHUTDOWNING = 1 << 6,
		F_WRITE_ERROR = 1 << 7,
		F_CLOSING=1<<8
	};

	#define WAWO_CHANNEL_CUSTOM_FLAG_BEGIN 12
	#define WAWO_CHANNEL_CUSTOM_FLAG_END 15

	class channel_pipeline;
	typedef SOCKET channel_id_t;

	class channel;
	typedef std::function<void(WWRP<channel>const& ch)> fn_channel_initializer;

	class channel :
		public wawo::ref_base
	{
		WWRP<io_event_loop> m_io_event_loop;
		WWRP<channel_pipeline> m_pipeline;
		WWRP<channel_promise> m_ch_close_future;
		WWRP<ref_base>	m_ctx;
		int m_errno;

	public:
		channel(WWRP<io_event_loop> const& poller) :
			m_io_event_loop(poller),
			m_pipeline(NULL),
			m_ch_close_future(NULL),
			m_ctx(NULL),
			m_errno(0)
		{
			TRACE_CH_OBJECT("channel::channel()");
		}

		~channel() {
			TRACE_CH_OBJECT("channel::~channel()");
		}

		inline WWRP<io_event_loop> const& event_poller() const {
			return m_io_event_loop;
		}

		inline WWRP<channel_pipeline> const& pipeline() const {
			WAWO_ASSERT(m_pipeline != NULL);
			return m_pipeline;
		}

		inline WWRP<channel_future> ch_close_future() const {
			return m_ch_close_future;
		}

		template <class ctx_t>
		inline WWRP<ctx_t> get_ctx() const {
			return wawo::static_pointer_cast<ctx_t>(m_ctx);
		}
		inline void set_ctx(WWRP<ref_base> const& ctx) {
			m_ctx = ctx;
		}
		inline void ch_errno(int e) {
			WAWO_ASSERT(e != wawo::OK);
			m_errno = e;
		}
		inline int ch_get_errno() { return m_errno; }

		inline WWRP<channel_promise> make_promise() {
			return wawo::make_ref<channel_promise>(WWRP<channel>(this));
		}

#define CH_FIRE_ACTION_IMPL_PACKET_1(_NAME,_P) \
		__WW_FORCE_INLINE void ch_##_NAME(WWRP<packet> const& _P) { \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_pipeline->fire_##_NAME(_P); \
		} \

		CH_FIRE_ACTION_IMPL_PACKET_1(read, income)

#define CH_FIRE_ACTION_IMPL_0(_NAME) \
		__WW_FORCE_INLINE void ch_fire_##_NAME() { \
			WAWO_ASSERT(m_io_event_loop != NULL ); \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_io_event_loop->execute([p=m_pipeline]() { \
				p->fire_##_NAME(); \
			}); \
		} \

		CH_FIRE_ACTION_IMPL_0(connected)
		CH_FIRE_ACTION_IMPL_0(read_shutdowned)
		CH_FIRE_ACTION_IMPL_0(write_shutdowned)

		CH_FIRE_ACTION_IMPL_0(write_block)
		CH_FIRE_ACTION_IMPL_0(write_unblock)

		inline void ch_fire_open() {
			WAWO_ASSERT(m_io_event_loop != NULL);
			m_pipeline = wawo::make_ref<channel_pipeline>(WWRP<channel>(this));
			m_pipeline->init();
			WAWO_ALLOC_CHECK(m_pipeline, sizeof(channel_pipeline));
			WAWO_ASSERT(m_ch_close_future == NULL);
			m_ch_close_future = wawo::make_ref<channel_promise>(WWRP<channel>(this));
		}

		inline void ch_fire_close(int const& code ) {
			WAWO_ASSERT(m_io_event_loop != NULL);
			WAWO_ASSERT(m_pipeline != NULL);
			WAWO_ASSERT(m_ch_close_future != NULL);
			m_ch_close_future->set_success(code);

			m_pipeline->fire_closed();
			m_pipeline->deinit();
			m_pipeline = NULL;
			m_ch_close_future = NULL;
		}

#define CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(NAME) \
private: \
		inline void _ch_##NAME(WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
			if (m_pipeline == NULL) { \
				event_poller()->schedule([ch_promise](){ \
					ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
				}); \
				return; \
			} \
			m_pipeline->##NAME(ch_promise); \
		} \
public: \
		inline WWRP<channel_future> ch_##NAME() {\
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this)); \
			return channel::ch_##NAME(ch_promise); \
		} \
		inline WWRP<channel_future> ch_##NAME(WWRP<channel_promise> const& ch_promise) { \
			WAWO_ASSERT( m_io_event_loop != NULL ); \
			m_io_event_loop->execute([_ch=WWRP<channel>(this), ch_promise]() { \
				_ch->_ch_##NAME(ch_promise); \
			}); \
			return ch_promise; \
		} \

		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(close)
		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(shutdown_read)
		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(shutdown_write)

#define CH_FUTURE_ACTION_IMPL_PACKET_1_CH_PROMISE_1(_NAME) \
private: \
		inline void _ch_##_NAME(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
			if (m_pipeline == NULL) { \
				event_poller()->schedule([ch_promise](){ \
					ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
				}); \
				return; \
			} \
			m_pipeline->##_NAME(outlet,ch_promise); \
		} \
public: \
		inline WWRP<channel_future> ch_##_NAME(WWRP<packet> const& outlet) {\
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this)); \
			return ch_##_NAME(outlet,ch_promise); \
		} \
		inline WWRP<channel_future> ch_##_NAME(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop != NULL); \
			m_io_event_loop->execute([_ch=WWRP<channel>(this), outlet, ch_promise]() { \
				_ch->_ch_##_NAME(outlet, ch_promise); \
			}); \
			return ch_promise; \
		} \
		
		CH_FUTURE_ACTION_IMPL_PACKET_1_CH_PROMISE_1(write);


#define CH_ACTION_IMPL_VOID(NAME) \
private: \
		inline void _ch_##NAME() { \
			WAWO_ASSERT(m_io_event_loop->in_event_loop()); \
			if (m_pipeline == NULL) { \
				return; \
			} \
			m_pipeline->##NAME(); \
		} \
public: \
		inline void ch_##NAME() {\
			WAWO_ASSERT( m_io_event_loop != NULL ); \
			m_io_event_loop->execute([_ch=WWRP<channel>(this)]() { \
				_ch->_ch_##NAME(); \
			}); \
		} \

		CH_ACTION_IMPL_VOID(flush)

		virtual channel_id_t ch_id() const = 0; //called by context in event_poller
		
		virtual void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_flush_impl() = 0;
		virtual void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_close_impl(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void ch_set_read_buffer_size(u32_t size) = 0;
		virtual void ch_set_read_buffer_size(u32_t size, WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_get_read_buffer_size(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void ch_set_write_buffer_size(u32_t size) = 0;
		virtual void ch_set_write_buffer_size(u32_t size, WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_get_write_buffer_size(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void ch_set_nodelay(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void begin_read(u8_t const& async_flag = 0, fn_io_event const& fn_read = NULL) = 0;
		virtual void end_read() = 0;

		virtual void begin_write(fn_io_event const& fn_write = NULL) = 0;
		virtual void end_write() = 0;

		virtual bool ch_is_active() const = 0;
	};
}}
#endif