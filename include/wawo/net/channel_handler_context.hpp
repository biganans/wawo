#ifndef _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP
#define _WAWO_NET_CHANNEL_HANDLER_CONTEXT_HPP

#include <wawo/net/channel_invoker.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/io_event.hpp>

namespace wawo { namespace net {

	enum channel_handler_flag {
		CH_ACCEPTOR	= 0x01,
		CH_ACTIVITY	= 0x02,
		CH_INBOUND	= 0x04,
		CH_OUTBOUND	= 0x08,
	};

	class channel_handler_context :
		public ref_base,
		public channel_acceptor_invoker_abstract,
		public channel_activity_invoker_abstract,
		public channel_inbound_invoker_abstract,
		public channel_outbound_invoker_abstract
	{
		friend class channel_pipeline;

		WWRP<channel_handler_context> P;
		WWRP<channel_handler_context> N;
		WWRP<channel_handler_abstract> m_h;
		u8_t m_flag;
	public:
		WWRP<channel> ch;

		channel_handler_context(WWRP<channel> const& ch, WWRP<channel_handler_abstract> const& h);
		virtual ~channel_handler_context();

		void fire_accepted(WWRP<channel> const& ch);
		void invoke_accepted(WWRP<channel> const& ch);

		void fire_connected();
		void invoke_connected();

		void fire_closed();
		void invoke_closed();

		void fire_error();
		void invoke_error();

		void fire_read_shutdowned();
		void invoke_read_shutdowned();

		void fire_write_shutdowned();
		void invoke_write_shutdowned();

		void fire_write_block();
		void invoke_write_block();

		void fire_write_unblock();
		void invoke_write_unblock();

		void fire_read(WWRP<packet> const& income);
		void invoke_read(WWRP<packet> const& income);

		int write(WWRP<packet> const& outlet);
		int invoke_write(WWRP<packet> const& outlet);

		int close(int const& code = 0);
		int invoke_close(int const& code);

		int close_read(int const& code = 0);
		int invoke_close_read(int const& code);

		int close_write(int const& code = 0);
		int invoke_close_write(int const& code);

		//void begin_connect(WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_connected = NULL, fn_io_event_error const& fn_err = NULL) ;
		//void end_connect() ;

		void begin_read(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_read = NULL, fn_io_event_error const& fn_err = NULL) ;
		void end_read() ;

		void begin_write(u8_t const& async_flag = 0, WWRP<ref_base> const& cookie = NULL, fn_io_event const& fn_write = NULL, fn_io_event_error const& fn_err = NULL) ;
		void end_write();
	};

#define HANDLER_CONTEXT_IMPL_H_TO_T_CHANNEL_1(CTX_CLASS_NAME,CHANNEL_NAME,NAME,HANDLER_CLASS_NAME) \
	void CTX_CLASS_NAME::fire_##NAME##( WWRP<CHANNEL_NAME> const& ch_ ) \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(ch_); \
		} \
	} \
	void CTX_CLASS_NAME::invoke_##NAME##( WWRP<CHANNEL_NAME> const& ch_) \
	{ \
		if (m_flag&CH_ACCEPTOR) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<CTX_CLASS_NAME>(this), ch_); \
		} else { \
			fire_##NAME##(ch_); \
		} \
	}

#define HANDLER_CONTEXT_IMPL_H_TO_T_0(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void CTX_CLASS_NAME::fire_##NAME##() \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(); \
		} \
	} \
	 \
	void CTX_CLASS_NAME::invoke_##NAME##() \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<CTX_CLASS_NAME>(this)); \
		} \
		else { \
			fire_##NAME##(); \
		} \
	}

#define HANDLER_CONTEXT_IMPL_H_TO_T_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	void CTX_CLASS_NAME::fire_##NAME##( WWRP<packet> const& p ) \
	{ \
		if (N != NULL) { \
			N->invoke_##NAME##(p); \
		} \
	} \
	 \
	void CTX_CLASS_NAME::invoke_##NAME##(WWRP<packet> const& p) \
	{ \
		WAWO_ASSERT(m_h != NULL); \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			_h->##NAME##(WWRP<CTX_CLASS_NAME>(this),p); \
		} \
		else { \
			fire_##NAME##(p); \
		} \
	}

#define INT_HANDLER_CONTEXT_IMPL_T_TO_H_PACKET_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	int CTX_CLASS_NAME::invoke_##NAME##(WWRP<packet> const& p) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			return _h->##NAME##(WWRP<CTX_CLASS_NAME>(this), p); \
		} else { \
			return NAME##(p); \
		} \
	} \
	 \
	int CTX_CLASS_NAME::##NAME##(WWRP<packet> const& p) { \
		WAWO_ASSERT(P != NULL); \
		return P->invoke_##NAME##(p); \
	}

#define INT_HANDLER_CONTEXT_IMPL_T_TO_H_INT_1(CTX_CLASS_NAME,NAME,HANDLER_FLAG,HANDLER_CLASS_NAME) \
	int CTX_CLASS_NAME::invoke_##NAME##(int const& i) { \
		if (m_flag&HANDLER_FLAG) { \
			WWRP<HANDLER_CLASS_NAME> _h = wawo::dynamic_pointer_cast<HANDLER_CLASS_NAME>(m_h); \
			return _h->##NAME##(WWRP<CTX_CLASS_NAME>(this), i); \
		} else { \
			return NAME##(i); \
		} \
	} \
	 \
	int CTX_CLASS_NAME::##NAME##(int const& i) { \
		WAWO_ASSERT(P != NULL); \
		return P->invoke_##NAME##(i); \
	}

}}
#endif