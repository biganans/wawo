#include <wawo/net/channel.hpp>
#include <wawo/net/channel_handler.hpp>
#include <wawo/net/channel_pipeline.hpp>
#include <wawo/net/channel_handler_context.hpp>

namespace wawo { namespace net {

	HANDLER_DEFAULT_IMPL_0(connected, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(closed, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(error, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(read_shutdowned, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(write_shutdowned, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(write_block, channel_activity_handler_abstract)
	HANDLER_DEFAULT_IMPL_0(write_unblock, channel_activity_handler_abstract)

	INT_HANDLER_DEFAULT_IMPL_0(close, channel_outbound_handler_abstract)
	INT_HANDLER_DEFAULT_IMPL_0(close_read, channel_outbound_handler_abstract)
	INT_HANDLER_DEFAULT_IMPL_0(close_write, channel_outbound_handler_abstract)

	//void channel_handler_head::accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch)
	//{
	//	ctx->fire_accepted( newch );
	//}

	//void channel_handler_head::read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income) 
	//{
	//	ctx->fire_read(income);
	//}
	
	int channel_handler_head::write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet)
	{
		return ctx->ch->ch_write(outlet);
	}
	
	int channel_handler_head::close(WWRP<channel_handler_context> const& ctx) {
		return ctx->ch->ch_close();
	}

	int channel_handler_head::close_read(WWRP<channel_handler_context> const& ctx) {
		return ctx->ch->ch_close_read();
	}

	int channel_handler_head::close_write(WWRP<channel_handler_context> const& ctx) {
		return ctx->ch->ch_close_write();
	}
	
	//--
	//void channel_handler_tail::accepted(WWRP<channel_handler_context> const& ctx, WWRP<channel> const& newch )
	//{
	//	ctx->fire_accepted(newch);
	//}

	//void channel_handler_tail::read(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& income)
	//{
	//	ctx->fire_read(income);
	//}

	//int channel_handler_tail::write(WWRP<channel_handler_context> const& ctx, WWRP<packet> const& outlet)
	//{
	//	WAWO_ASSERT(!"socket_handler_head::write,,, send a flush ?");
	//	return wawo::OK;

	//	(void)ctx;
	//	(void)outlet;
	//}

}}