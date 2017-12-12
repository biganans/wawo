
#include <wawo/net/peer/message/mux_cargo.hpp>

namespace wawo { namespace net { namespace peer {

	void stream::_flush(WWRP<socket> const& so, int& ec_o, int& wflag ) {

		WAWO_ASSERT(so != NULL);
		ec_o = wawo::OK;
		wflag = 0;

#ifdef ENABLE_STREAM_WND
		//check wnd update
		{
			lock_guard<spin_mutex> lg_rb(rb_mutex);
			if (rb_incre > 0) {
				WWSP<packet> opack = wawo::make_shared<packet>();
				opack->write<u32_t>(rb_incre);
				WWSP<message::mux_cargo> m = wawo::make_shared<message::mux_cargo>(WWRP<stream>(this), message::T_UWND, opack);
				WWSP<packet> mpack;
				ec_o = m->encode(mpack);
				WAWO_ASSERT(ec_o == wawo::OK);
				ec_o = so->send_packet(mpack);
				WAWO_RETURN_IF_MATCH(ec_o != wawo::OK);
				DEBUG_STREAM("[mux_cargo][s%u]update incre %u -> 0", id, rb_incre );
				rb_incre = 0;
			}
		}
#endif

		if ( rb_incre == 0 && ((flag&(STREAM_WRITE_SHUTDOWN | STREAM_READ_SHUTDOWN))== (STREAM_WRITE_SHUTDOWN | STREAM_READ_SHUTDOWN)) ) {
			DEBUG_STREAM("[mux_cargo][s%u]update state to SS_CLOSE", id);
			state = SS_CLOSED;
			return;
		}

		if (flag&STREAM_WRITE_SHUTDOWN) {
			return;
		}

		if (wb->count() == 0 ) {
			if (flag&STREAM_PLAN_FIN) {
				WWSP<message::mux_cargo> m = wawo::make_shared<message::mux_cargo>(WWRP<stream>(this), message::T_FIN);
				WWSP<packet> mpack;
				int encrt = m->encode(mpack);
				WAWO_ASSERT(encrt == wawo::OK);
				ec_o = so->send_packet(mpack);

				if (ec_o == wawo::OK) {
					lock_guard<spin_mutex> lg_self(mutex);
					flag &= ~STREAM_PLAN_FIN;
					flag |= STREAM_WRITE_SHUTDOWN;
					DEBUG_STREAM("[mux_cargo][s%u]stream send T_FIN", id);
				}
			}
			return;
		}

		if ( !(flag&STREAM_BUFFER_DETERMINED) ) {
			flag |= STREAM_BUFFER_DETERMINED;

			socket_buffer_cfg cfg = so->get_buffer_cfg();
			u32_t try_size = (cfg.snd_size - 1024);

			u32_t size = WAWO_MIN(try_size, wb->capacity());

			lock_guard<spin_mutex> lg_s(wb_mutex);
			if (size != wb->capacity()) {
				WWRP<bytes_ringbuffer> sb_new = wawo::make_ref<bytes_ringbuffer>(size);
				while (wb->count()) {
					byte_t tmp[8192];
					u32_t nbytes = wb->read(tmp, sizeof(tmp) / sizeof(byte_t));
					sb_new->write(tmp, nbytes);
				}
				WAWO_ASSERT(wb->count() == 0);
				wb = sb_new;
			}
		}

		if (flag&STREAM_PLAN_SYN) {
			WWSP<message::mux_cargo> m = wawo::make_shared<message::mux_cargo>(WWRP<stream>(this), message::T_SYN);
			WWSP<packet> mpack;
			ec_o = m->encode(mpack);
			WAWO_ASSERT(ec_o == wawo::OK);
			ec_o = so->send_packet(mpack);
			WAWO_RETURN_IF_MATCH(ec_o != wawo::OK);
			lock_guard<spin_mutex> lg_self(mutex);
			flag &= ~STREAM_PLAN_SYN;
		}

		u32_t flushed = 0;
		u32_t flushed_max_each_time = MUX_STREAM_WND_SIZE;
		lock_guard<spin_mutex> lg_s(wb_mutex);
		while (wb->count() && (flushed < flushed_max_each_time) 
#ifdef ENABLE_STREAM_WND
			&& (wb_wnd>0)
#endif	 
		)
		{
			u32_t can_send_max = ((flushed_max_each_time - flushed));

#ifdef ENABLE_STREAM_WND
			if (can_send_max > wb_wnd) {
				can_send_max = wb_wnd;
			}
#endif
			WWSP<packet> opack = wawo::make_shared<packet>(can_send_max);

			u32_t nbytes = wb->peek(opack->begin(), can_send_max );
			WAWO_ASSERT( (nbytes > 0) && nbytes <= can_send_max );
			opack->forward_write_index(nbytes);

			WWSP<message::mux_cargo> m = wawo::make_shared<message::mux_cargo>(WWRP<stream>(this), message::T_DATA, opack);
			WWSP<packet> mpack;
			ec_o = m->encode(mpack);
			WAWO_ASSERT(ec_o == wawo::OK);

			ec_o = so->send_packet(mpack);
			if ( ec_o != wawo::OK ) {
				break;
			} else {

				//DEBUG_STREAM("[mux_cargo][s%u]flushed bytes: %u", id, nbytes);
				wb->skip(nbytes);
				flushed += nbytes;
				WAWO_ASSERT(wb_wnd >= nbytes);

#ifdef ENABLE_STREAM_WND
				wb_wnd -= nbytes;
#endif
			}
		}

		DEBUG_STREAM("[mux_cargo][s%u]flushed: %u, left: %u, wb_wnd: %u, ec_o: %d", id, flushed, wb->count() , wb_wnd, ec_o );

		if ((flushed>0) && (write_flag&STREAM_WRITE_BLOCKED) ) {
			wflag |= STREAM_WRITE_UNBLOCKED;

			write_flag &= ~STREAM_WRITE_BLOCKED;
			wflag |= STREAM_WRITE_BLOCKED;
		}
	}
}}}