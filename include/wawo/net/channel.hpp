#ifndef _WAWO_NET_CHANNEL_HPP
#define _WAWO_NET_CHANNEL_HPP

#include <wawo/packet.hpp>
#include <wawo/net/io_event_loop.hpp>

#include <wawo/net/channel_future.hpp>
#include <wawo/net/channel_pipeline.hpp>

namespace wawo { namespace net {
	class channel_pipeline;

	class channel :
		public wawo::ref_base
	{
		WWRP<channel_pipeline> m_pipeline;
		WWRP<ref_base>	m_ctx;
		WWRP<io_event_loop> m_io_event_loop;
		WWRP<channel_promise> m_ch_close_future;
		int m_errno;

	protected:
		inline WWRP<channel_promise> ch_close_promise() {
			return m_ch_close_future;
		}
	public:
		channel(WWRP<io_event_loop> const& poller) :
			m_pipeline(NULL),
			m_ctx(NULL),
			m_io_event_loop(poller),
			m_ch_close_future(NULL),
			m_errno(0)
		{}
		~channel() {}

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

		inline WWRP<io_event_loop> event_poller() const {
			return m_io_event_loop;
		}

		inline WWRP<channel_pipeline>& pipeline() {
			WAWO_ASSERT(m_pipeline != NULL);
			return m_pipeline;
		}

		inline WWRP<channel_future> ch_close_future() const {
			return m_ch_close_future;
		}

#define CH_FIRE_ACTION_IMPL_PACKET_1(_NAME,_P) \
		void ch_##_NAME(WWRP<packet> const& _P) { \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_pipeline->fire_##_NAME(_P); \
		} \

		CH_FIRE_ACTION_IMPL_PACKET_1(read, income)

#define CH_FIRE_ACTION_IMPL_0(_NAME) \
		void ch_fire_##_NAME() { \
			WAWO_ASSERT(m_io_event_loop != NULL ); \
			WAWO_ASSERT(m_pipeline != NULL); \
			m_io_event_loop->execute([p=m_pipeline]() { \
				p->fire_##_NAME(); \
			}); \
		} \

			CH_FIRE_ACTION_IMPL_0(connected)
			CH_FIRE_ACTION_IMPL_0(error)
			CH_FIRE_ACTION_IMPL_0(read_shutdowned)
			CH_FIRE_ACTION_IMPL_0(write_shutdowned)

			CH_FIRE_ACTION_IMPL_0(write_block)
			CH_FIRE_ACTION_IMPL_0(write_unblock)

			inline void ch_fire_opened() {
			WAWO_ASSERT(m_io_event_loop != NULL);
			m_pipeline = wawo::make_ref<channel_pipeline>(WWRP<channel>(this));
			m_pipeline->init();
			WAWO_ALLOC_CHECK(m_pipeline, sizeof(channel_pipeline));
			WAWO_ASSERT(m_ch_close_future == NULL);
			m_ch_close_future = wawo::make_ref<channel_promise>(WWRP<channel>(this));
		}

		inline void ch_fire_closed() {
			WAWO_ASSERT(m_io_event_loop != NULL);
			WAWO_ASSERT(m_pipeline != NULL);
			m_pipeline->fire_closed();
			m_pipeline->deinit();
			m_pipeline = NULL;
		}

		inline bool is_ch_closed() const {
			return m_pipeline == NULL;
		}

#define CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(NAME) \
private: \
		inline void _ch_##NAME(WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop->in_poller()); \
			if (m_pipeline == NULL) { \
				ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
				return; \
			} \
			m_pipeline->##NAME(ch_promise); \
		} \
public: \
		WWRP<channel_future> ch_##NAME() {\
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(WWRP<channel>(this)); \
			return channel::ch_##NAME(ch_promise); \
		} \
		WWRP<channel_future> ch_##NAME(WWRP<channel_promise> const& ch_promise) { \
			WAWO_ASSERT( m_io_event_loop != NULL ); \
			if( !m_io_event_loop->in_poller() ) { \
				WWRP<channel> _ch(this); \
				m_io_event_loop->schedule([_ch, ch_promise]() { \
					_ch->_ch_##NAME(ch_promise); \
				}); \
			} else {\
				_ch_##NAME(ch_promise); \
			} \
			return ch_promise; \
		} \

		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(close)
		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(shutdown_read)
		CH_FUTURE_ACTION_IMPL_CH_PROMISE_1(shutdown_write)

#define CH_FUTURE_ACTION_IMPL_PACKET_1_CH_PROMISE_1(_NAME) \
private: \
			inline void _ch_##_NAME(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop->in_poller()); \
			if (m_pipeline == NULL) { \
				ch_promise->set_success(wawo::E_CHANNEL_CLOSED_ALREADY); \
				return; \
			} \
			m_pipeline->##NAME(ch_promise,ch_promise); \
		} \
public: \
		WWRP<channel_future> ch_##_NAME(WWRP<packet> const& outlet) {\
			WWRP<channel_promise> ch_promise = wawo::make_ref<channel_promise>(); \
			return ch_##_NAME(outlet,ch_promise); \
		} \
		WWRP<channel_future> ch_##_NAME(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) {\
			WAWO_ASSERT(m_io_event_loop != NULL); \
			if(!m_io_event_loop->in_poller()) { \
				WWRP<channel> _ch(this); \
				m_io_event_loop->schedule([_ch, outlet, ch_promise]() { \
					_ch->_ch_##_NAME(outlet, ch_promise); \
				}); \
			} else { \
				_ch_##_NAME(outlet,ch_promise); \
			} \
			return ch_promise; \
		} \
		CH_FUTURE_ACTION_DECL_PACKET_1_CH_PROMISE_1(write);


#define CH_ACTION_IMPL_VOID(NAME) \
private: \
	inline void _ch_##NAME() { \
			WAWO_ASSERT(m_io_event_loop->in_poller()); \
			if (m_pipeline == NULL) { \
				return; \
			} \
			m_pipeline->##NAME(); \
		} \
public: \
		void ch_##NAME() {\
			WAWO_ASSERT( m_io_event_loop != NULL ); \
			if(!m_io_event_loop->in_poller()) { \
				WWRP<channel> _ch(this); \
				m_io_event_loop->schedule([_ch]() { \
					_ch->_ch_##NAME(); \
				}); \
			} else {\
				_ch_##NAME(); \
			}\
		} \

		CH_ACTION_IMPL_VOID(flush)

		//could be called directly in any place
		virtual int ch_id() const = 0; //called by context in event_poller
		
		virtual void ch_write_impl(WWRP<packet> const& outlet, WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_flush_impl() = 0;
		virtual void ch_shutdown_read_impl(WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_shutdown_write_impl(WWRP<channel_promise> const& ch_promise) = 0;
		virtual void ch_close_impl(WWRP<channel_promise> const& ch_promise) = 0;

		virtual void begin_read(u8_t const& async_flag = 0, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) {
			(void)async_flag;
			(void)fn_read;
			(void)fn_err;
		}
		virtual void end_read() {}

		virtual void begin_write(u8_t const& async_flag =0, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL) {
			(void)async_flag;
			(void)fn_write;
			(void)fn_err;
		}
		virtual void end_write() {}

		virtual int turnon_nodelay() { return wawo::OK; }
		virtual int turnoff_nodelay() { return wawo::OK; }
		
		virtual bool is_active() const = 0;

		/*virtual bool is_read_shutdowned() const = 0;
		virtual bool is_write_shutdowned() const = 0;
		virtual bool is_readwrite_shutdowned() const = 0;
		virtual bool is_closed() const = 0;
		*/
	};
}}
#endif