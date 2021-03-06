#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/channel.hpp>

namespace wawo { namespace net {

	channel_pipeline::channel_pipeline(WWRP<channel> const& ch_ ):
		m_ch(ch_),
		m_io_event_loop(ch_->event_poller())
	{
		TRACE_CH_OBJECT("channel_pipeline::channel_pipeline()");
	}

	channel_pipeline::~channel_pipeline()
	{
		TRACE_CH_OBJECT("channel_pipeline::~channel_pipeline()");
	}

	void channel_pipeline::init()
	{
		WWRP<channel_handler_head> h = wawo::make_ref<channel_handler_head>();
		m_head = wawo::make_ref<channel_handler_context>(m_ch, h);

		WWRP<channel_handler_tail> t = wawo::make_ref<channel_handler_tail>();
		m_tail = wawo::make_ref<channel_handler_context>(m_ch, t);

		m_head->P = NULL;
		m_head->N = m_tail;
		m_tail->P = m_head;
		m_tail->N = NULL;
	}

	void channel_pipeline::deinit()
	{
		WWRP<channel_handler_context> _hctx = m_tail;
		while (_hctx != NULL ) {
			_hctx->m_flag |= (CH_REMOVED|CH_CH_CLOSED);
			_hctx->N = NULL;
			WWRP<channel_handler_context> TMP = _hctx->P;
			_hctx->P = NULL;
			_hctx = TMP;
		}
	}

	/*
	WWRP<channel_pipeline> channel_pipeline::add_last(WWRP<channel_handler_abstract> const& h) {
		WAWO_ASSERT(m_io_event_loop != NULL);
		WWRP<channel_pipeline> _this(this);
		if (m_io_event_loop->in_event_loop()) {


			return _this;
		}

		m_io_event_loop->execute([P= WWRP<channel_pipeline>(this), h]() -> void {
			P->add_last(h);
		});
		return _this;
	}
	*/

}}