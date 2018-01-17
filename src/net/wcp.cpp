
#include <wawo/net/socket_observer.hpp>
#include <wawo/net/socket.hpp>
#include <wawo/net/wcp.hpp>

#define WCPPACK_TO_UDPMESSAGE( wcp_pack, mbuffer, size, wlen ) \
do { \
	WAWO_ASSERT(mbuffer != 0); \
	WAWO_ASSERT(WCP_HeaderLen<=size); \
	WAWO_ASSERT( (wcp_pack).data == NULL? true: (wcp_pack).data->len() <= WCP_MDU ); \
	wlen = 0; \
	wawo::bytes_helper::write_impl<wawo::u32_t>( (wcp_pack).header.seq, mbuffer + wlen); \
	wlen += sizeof(wawo::u32_t); \
	wawo::bytes_helper::write_impl<wawo::u32_t>((wcp_pack).header.ack, mbuffer + wlen); \
	wlen += sizeof(wawo::u32_t); \
	wawo::bytes_helper::write_impl<wawo::u32_t>((wcp_pack).header.wnd, mbuffer + wlen); \
	wlen += sizeof(wawo::u32_t); \
	wawo::bytes_helper::write_impl<wawo::u16_t>((wcp_pack).header.flag, mbuffer + wlen ); \
	wlen += sizeof(wawo::u16_t); \
	if ((wcp_pack).data != NULL && (wcp_pack).data->len()) { \
		wawo::bytes_helper::write_impl<wawo::u16_t>((wawo::u16_t)(wcp_pack).data->len(), mbuffer + wlen); \
		wlen += sizeof(wawo::u16_t); \
		WAWO_ASSERT(size>=(wlen + (wcp_pack).data->len())); \
		::memcpy( (void*) (mbuffer+wlen), (void*)(wcp_pack).data->begin(), (wcp_pack).data->len() ); \
		wlen += ((wcp_pack).data->len())&0xFFFF; \
	} else { \
		wawo::bytes_helper::write_impl<wawo::u16_t>(0, mbuffer + wlen); \
		wlen += sizeof(wawo::u16_t); \
	}\
} while (0)

#define WCP_RECEIVED_PACK_FROM_UDPMESSAGE( pack, mbuffer,len) \
do { \
	WAWO_ASSERT(mbuffer != 0); \
	WAWO_ASSERT(WCP_HeaderLen<=len); \
	wawo::u32_t rlen = 0; \
	(pack).header.seq = wawo::bytes_helper::read_u32(mbuffer + rlen); \
	rlen += sizeof(wawo::u32_t); \
	(pack).header.ack = wawo::bytes_helper::read_u32(mbuffer + rlen); \
	rlen += sizeof(wawo::u32_t); \
	(pack).header.wnd = wawo::bytes_helper::read_u32(mbuffer + rlen); \
	rlen += sizeof(wawo::u32_t); \
	(pack).header.flag = wawo::bytes_helper::read_u16(mbuffer + rlen ); \
	rlen += sizeof(wawo::u16_t); \
	(pack).header.dlen = wawo::bytes_helper::read_u16(mbuffer + rlen); \
	rlen += sizeof(wawo::u16_t); \
	if ((pack).header.dlen != 0) { \
		WAWO_ASSERT((pack).header.dlen<=WCP_MDU); \
		WAWO_ASSERT((pack).header.dlen==(len-rlen)); \
		WWSP<wawo::packet> data = wawo::make_shared<wawo::packet>((pack).header.dlen); \
		data->write(mbuffer+rlen, (pack).header.dlen ); \
		(pack).data = data; \
		rlen += (pack).header.dlen; \
	} \
} while (0)

namespace wawo { namespace net {

	void wcb_pump_packs(WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL );
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<WCB> wcb = wawo::static_pointer_cast<WCB>(cookie->user_cookie);
		WAWO_ASSERT(wcb != NULL);
		wcb->pump_packs();
	}

	void wcb_socket_error(int const& code, WWRP<ref_base> const& cookie_) {
		WAWO_ASSERT(cookie_ != NULL);
		WWRP<async_cookie> cookie = wawo::static_pointer_cast<async_cookie>(cookie_);
		WWRP<WCB> wcb = wawo::static_pointer_cast<WCB>(cookie->user_cookie);
		WAWO_ASSERT(wcb->so != NULL);
		wcb->so->close(code);
		WAWO_ERR("[wcp][wcb][#%d:%s]wcb_socket_error: %d", wcb->so->get_fd(), wcb->so->get_addr_info().cstr, code );
	}

	inline wawo::u32_t four_tuple_hash(wawo::net::address const& addr1, wawo::net::address const& addr2) {
		return ((wawo::u32_t)(addr1.get_hostsequence_ulongip()) * 59) ^
			((wawo::u32_t)(addr2.get_hostsequence_ulongip())) ^
			((wawo::u32_t)(addr1.get_port()) << 16) ^
			((wawo::u32_t)(addr2.get_port()))
			;
	}

	inline static int inject_to_address(WWRP<socket> const& so, WWSP<WCB_pack> const& opack, address const& to) {
		WAWO_ASSERT(!to.is_null());
		byte_t buffer[WCP_MTU];
		u16_t len;
		WCPPACK_TO_UDPMESSAGE(*opack, buffer, WCP_MTU, len);
		int ec;
		wawo::u32_t nbytes = so->sendto(buffer, len, to, ec);

#ifdef WCP_TRACE_INOUT_PACK
		WCP_TRACE("[wcp]WCB::inject_to_address, sendto: %s, seq: %u, flag: %u, ack: %u, wnd: %u, sndrt: %d",
			to.address_info().cstr, opack->header.seq, opack->header.flag, opack->header.ack, opack->header.wnd, ec );
#endif
		WAWO_RETURN_V_IF_MATCH(ec, ((ec == wawo::OK) && (nbytes == len)) );
		WAWO_WARN("[wcp]WCB::send_pack_to_address failed: %d, sent: %u, sendto: %s, seq: %u, flag: %u, ack: %u, wnd: %u",
			ec, nbytes, to.address_info().cstr, opack->header.seq, opack->header.flag, opack->header.ack, opack->header.wnd);
		return ec;
	}

	inline static int reply_rst_to_address(WWRP<socket> const& so, u32_t const& seq, address const& to) {
		WWSP<WCB_pack> rst = wawo::make_shared<WCB_pack>();
		rst->header.seq = seq;
		rst->header.flag = WCP_FLAG_RST;
		rst->header.dlen = 0;

		WCP_TRACE("[wcp][rst]reply rst from: %d:%s to %s", so->get_fd(), so->get_addr_info().cstr, to.address_info().cstr );
		return inject_to_address(so, rst, to);
	}

	WCB::WCB() {}
	WCB::~WCB() {}

	WCB_State WCB::update(u64_t const& now) {

		switch (state) {

		case WCB_CLOSED:
		{
			//WCB can only be recycled after the call of close
			if ((r_flag&READ_LOCAL_READ_SHUTDOWNED) && (s_flag&WRITE_LOCAL_WRITE_SHUTDOWNED)) {
				lock_guard<spin_mutex> lg(mutex);
				if (wcb_flag&WCB_FLAG_CLOSED_CALLED) {
					state = WCB_RECYCLE;
				}
			}
		}
		break;
		case WCB_LISTEN:
		{
			WAWO_ASSERT(wcb_flag&WCB_FLAG_IS_LISTENER);
			if (!(r_flag&READ_LOCAL_READ_SHUTDOWNED)) {
				listen_handle_syn();
			}

			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			WCBList::iterator it = backloglist_pending.begin();
			while (it != backloglist_pending.end()) {
				WCB_State s = (*it)->update(now);
				if (s == WCB_ESTABLISHED) {
					WAWO_ASSERT(backlogq.size() <= backlog_size);
					backlogq.push(*it);
					it = backloglist_pending.erase(it);
				}
				else if (s == WCB_CLOSE_WAIT ||
					s == WCB_CLOSED
					) {
					(*it)->close();
					it = backloglist_pending.erase(it);
				}
				else if (s == WCB_SYN_RECEIVED) {
					++it;
				}
				else {
					WAWO_THROW("wcp logic issue");
				}
			}
		}
		break;
		case WCB_SYNING:
		{
			check_flights(now);
			check_send(now);
		}
		break;
		case WCB_SYN_SENT:
		{
			if ((now - timer_state) >= WCP_SYN_MAX_TIME) {
				lock_guard<spin_mutex> lg(mutex);
				state = WCB_CLOSED;
				wcb_errno = wawo::E_ETIMEOUT;
				cond.notify_all();
			} else {
				check_recv(now);
				check_flights(now);
				check_send(now);
			}
		}
		break;
		case WCB_SYN_RECEIVED:
		case WCB_ESTABLISHED:
		case WCB_CLOSE_WAIT:
		{
			if (wcb_errno != 0) {
				lock_guard<spin_mutex> lg(mutex);
				state = WCB_CLOSED;
			}
			else {
				check_recv(now);
				check_flights(now);
				check_send(now);
			}
		}
		break;
		case WCB_FIN_WAIT_1:
		case WCB_CLOSING:
		case WCB_FIN_WAIT_2:
		{
			if (wcb_errno != 0 || (now - timer_state) >= WCP_FIN_MAX_TIME) {
				lock_guard<spin_mutex> lg(mutex);
				state = WCB_CLOSED;
			}
			else {
				check_recv(now);
				check_flights(now);
				check_send(now);
			}
		}
		break;
		case WCB_LAST_ACK:
		{
			check_recv(now);
			check_flights(now);
			check_send(now);

			if (wcb_errno != 0 || (now - timer_state) >= WCP_LAST_ACK_MAX_TIME) {
				lock_guard<spin_mutex> lg(mutex);
				state = WCB_CLOSED;
			}
		}
		break;
		case WCB_TIME_WAIT:
		{
			if ((now - timer_state) >= WCP_TIMEWAIT_MAX_TIME) {
				lock_guard<spin_mutex> lg(mutex);
				state = WCB_CLOSED;
			}
		}
		break;
		case WCB_RECYCLE:
		{
		}
		break;
		}

		return state;
	}

	void WCB::check_recv(u64_t const& now) {

		{
			if (received_vec->size()) { received_vec->clear(); }
			lock_guard<spin_mutex> lg_received_vec(received_vec_mutex);
			{
				if (received_vec_standby->size() != 0) {
					received_vec.swap(received_vec_standby);
				}
			}
		}

		u32_t received_size = received_vec->size();
		if (received_size == 0) {
			u64_t timediff = ((now - keepalive_timer_last_received_pack)/1000);
			if ( keepalive_probes_sent >= keepalive_vals.probes ) {
				lock_guard<spin_mutex> lg_r_mutex( r_mutex);
				r_flag |= READ_RECV_ERROR;
				wcb_errno = wawo::E_ETIMEOUT;
				WCP_TRACE("[wcp][%u:%s]connection timeout", fd, remote_addr.address_info().cstr );
			}
			else if (timediff >= static_cast<u64_t>(keepalive_vals.idle + (keepalive_vals.interval*keepalive_probes_sent))) {
				//lock_guard<spin_mutex> lg_s_mutex( s_mutex );
				keepalive();
				++keepalive_probes_sent;
				WCP_TRACE("[wcp][%u:%s]keepalive, probes: %u", fd, remote_addr.address_info().cstr, keepalive_probes_sent );
			}
			else {}
		}
		else {

			keepalive_timer_last_received_pack = now;
			keepalive_probes_sent = 0;

			std::queue<u32_t> acked_queue; //0 normal, 1 dup
			for (u32_t i = 0; i < received_size; ++i) {
				WWSP<WCB_received_pack> const& pack = (*received_vec)[i];

				//ack new means the packet has been read by remote user, so rwnd should be updated
				//@todo, we should check ack range
				if (snd_info.una < pack->header.ack) {
					snd_info.una = pack->header.ack;
					wcb_flag |= SND_UNA_UPDATE;
					snd_info.rwnd = pack->header.wnd;
				}

				if (WCPPACK_TEST_FLAG(*pack, WCP_FLAG_WND)) {
					snd_info.rwnd = pack->header.wnd;
				}

				if (WCPPACK_TEST_FLAG(*pack, WCP_FLAG_SACK)) {
					WAWO_ASSERT(pack->data != NULL && pack->data->len());
					while (pack->data->len()) {
						acked_queue.push(pack->data->read<u32_t>());
					}
					continue;
				}

				if (WCPPACK_TEST_FLAG(*pack, (WCP_FLAG_KEEP_ALIVE))) {
					keepalive_reply();
					WCP_TRACE("[wcp][%u:%s]keepalive reply, probes: %u", fd, remote_addr.address_info().cstr, keepalive_probes_sent);
					continue;
				}

				if (WCPPACK_TEST_FLAG(*pack, WCP_FLAG_KEEP_ALIVE_REPLY)) {
					continue;
				}

				snd_sacked_pack_tmp->write<u32_t>(pack->header.seq);

				if (pack->header.seq < rcv_info.next) {
					WCP_TRACE("[wcp]check_recv, duplicate (old), seq: %u, flag: %u, wnd: %u, ack: %u, expect: %u",
						pack->header.seq, pack->header.flag, pack->header.wnd, pack->header.ack, rcv_info.next);

					continue;
				}

				WCB_ReceivedPackList::iterator it = rcv_received.begin();
				while (it != rcv_received.end()) {
					u32_t& seq = (*it)->header.seq;
					if ( seq == pack->header.seq) {
						WCP_TRACE("[wcp]check_recv, duplicate (new), update rwnd, seq: %u, flag: %u, wnd: %u, ack: %u, expect: %u",
							pack->header.seq, pack->header.flag, pack->header.wnd, pack->header.ack, rcv_info.next);

						goto _end_insert_loop;
					}
					else if (seq < pack->header.seq) {
						++it;
					}
					else {
						break;
					}
				}
				rcv_received.insert(it, pack);
				//wcb_flag |= RCV_ARRIVE_NEW;
			_end_insert_loop:
				(void)it;//for compile grammar
			}

			if (snd_sacked_pack_tmp->len()) {
				//lock_guard<spin_mutex> lg_s_mutex(s_mutex);
				SACK(snd_sacked_pack_tmp);
				snd_sacked_pack_tmp->reset();
			}

			while (acked_queue.size()) {
				u32_t const& ack = acked_queue.front();

				if (ack < snd_info.una) {
					acked_queue.pop();
					continue;
				}

				if (ack > snd_biggest_ack) {
					//WCP_TRACE("[wcp]biggest_ack update, last: %u, now: %u, una: %u", snd_biggest_ack, ack, snd_info.una );
					snd_biggest_ack = ack;
					wcb_flag |= SND_BIGGEST_SACK_UPDATE;
				}

				WCB_PackList::iterator const& otf = std::find_if(snd_flights.begin(), snd_flights.end(), [&ack](WWSP<WCB_pack> const& pack /*on the flight*/) {
					return (pack->header.seq == ack);
				});

				if (otf != snd_flights.end()) {
					_handle_pack_acked(otf,now);
					snd_flights.erase(otf);
				}

				if ((wcb_flag&SND_LOST_DETECTED) && (ack > snd_lost_biggest_seq)) {
					++snd_lost_bigger_than_lost_seq_time;

					if ( (snd_lost_bigger_than_lost_seq_time >= 1) && (snd_info.una>snd_lost_biggest_seq) ) {

						snd_info.cwnd += snd_lost_cwnd_for_fast_recovery_compensation;
						snd_info.cwnd = WAWO_MIN(snd_info.cwnd, WCP_SND_CWND_MAX);

						WCP_TRACE("[wcp][FR]try to set ssthresh from: %u to: %u (choose larger one), compensation: %u", snd_info.ssthresh, snd_info.cwnd, snd_lost_cwnd_for_fast_recovery_compensation );

						u32_t _new_ssthresh = WAWO_MIN(snd_info.cwnd, WCP_SND_SSTHRESH_MAX);
						snd_info.ssthresh = WAWO_MAX( _new_ssthresh, snd_info.ssthresh );

						snd_lost_cwnd_for_fast_recovery_compensation = 0;
						wcb_flag |= SND_FAST_RECOVERED;
						wcb_flag &= ~SND_LOST_DETECTED;
					}
				}

				acked_queue.pop();
			}

			WCB_ReceivedPackList::iterator it = rcv_received.begin();
			while (it != rcv_received.end()) {
				WAWO_ASSERT((*it)->header.seq >= rcv_info.next);

				if ((*it)->header.seq != rcv_info.next) {
					break;
				}

				WWSP<WCB_received_pack> const& inpack = *it;

				if (inpack->header.dlen>0 ) {

					WAWO_ASSERT(WCPPACK_TEST_FLAG(*(inpack), WCP_FLAG_DAT));
					WAWO_ASSERT(!WCPPACK_TEST_FLAG(*(inpack), (WCP_FLAG_SYN | WCP_FLAG_FIN)));

					if (inpack->from != remote_addr) {
						reply_rst_to_address(so, inpack->header.ack, inpack->from);

						lock_guard<spin_mutex> lg_wcb(mutex);
						state = WCB_CLOSED;
						wcb_errno = wawo::E_ECONNABORTED;

						rcv_received.erase(it);
						break;
					}

					if (rb_standby->left_capacity() < inpack->header.dlen) {
						WAWO_WARN("[wcp]check recv, wcb rb buffer not enough, break, incoming dlen: %u, rb->left_capacity(): %u", inpack->header.dlen, rb_standby->left_capacity());
						r_flag |= READ_RWND_LESS_THAN_MTU;
						break;
					}

					rb_standby->write(inpack->data->begin(), inpack->header.dlen);
					rcv_info.wnd = rb_standby->left_capacity();
					if (rcv_info.wnd <= WCP_MTU) {
						r_flag |= READ_RWND_LESS_THAN_MTU;
					}
					else {
						r_flag &= ~(READ_RWND_LESS_THAN_MTU | READ_RWND_MORE_THAN_MTU);
					}
				}

				rcv_info.next++;

				if (WCPPACK_TEST_FLAG(*(inpack), WCP_FLAG_RST)) {
					lock_guard<spin_mutex> lg_wcp_state(mutex);

					wcb_errno = wawo::E_ECONNRESET;
					state = WCB_CLOSED;
					cond.notify_all();

					lock_guard<spin_mutex> lg_r_mutex(r_mutex);
					r_cond.notify_all();
					break;
				}

				if (WCPPACK_TEST_FLAG(*(inpack), WCP_FLAG_SYN)) {
					lock_guard<spin_mutex> lg_wcp_state(mutex);
					if (state == WCB_SYN_RECEIVED) {
						SYNSYNACK();
					}
					else if (state == WCB_SYN_SENT) {
						//update remote
						remote_addr = inpack->from;
						SYNACK();
					}
					else {
						reply_rst_to_address(so, inpack->header.ack, inpack->from);
						rcv_received.erase(it);

						wcb_errno = wawo::E_ECONNABORTED;
						state = WCB_CLOSED;
						cond.notify_all();
						break;
					}
				}

				if (WCPPACK_TEST_FLAG(*(inpack), WCP_FLAG_ACK)) {
					lock_guard<spin_mutex> lg_wcp_state(mutex);
					switch (state) {
					case WCB_SYN_SENT:
					{
						WAWO_ASSERT( !remote_addr.is_null() );
						WAWO_ASSERT( remote_addr == inpack->from );

						int connrt = so->connect(remote_addr);
						if (connrt == wawo::OK) {
							state = WCB_ESTABLISHED;
						}
						else {
							wcb_errno = connrt;
							state = WCB_CLOSED;
						}

						if (!(wcb_option&WCP_O_NONBLOCK)) {
							cond.notify_all();
						}
					}
					break;
					case WCB_SYN_RECEIVED:
					{
						state = WCB_ESTABLISHED;
					}
					break;
					case WCB_FIN_WAIT_1:
					{
						state = WCB_FIN_WAIT_2;
					}
					break;
					case WCB_LAST_ACK:
					{
						state = WCB_CLOSED;
					}
					break;
					case WCB_CLOSING:
					{
						timer_state = now;
						state = WCB_TIME_WAIT;
					}
					break;
					case WCB_ESTABLISHED:
					{
						//update una
					}
					break;
					case WCB_FIN_WAIT_2:
					{
						//do nothing
					}
					break;
					case WCB_CLOSE_WAIT:
					case WCB_TIME_WAIT:
					{}
					break;
					default:
					{
						WAWO_ASSERT(!"wcp state logic issue");
					}
					break;
					}
				}

				if (WCPPACK_TEST_FLAG(*(inpack), WCP_FLAG_FIN)) {
					lock_guard<spin_mutex> lg_wcp_state(mutex);

					switch (state) {
					case WCB_ESTABLISHED:
					{
						state = WCB_CLOSE_WAIT;
						lock_guard<spin_mutex> lg_r_mutex(r_mutex);
						r_flag |= READ_REMOTE_FIN_RECEIVED;
						r_flag &= ~READ_REMOTE_FIN_READ_BY_USER;

						cond.notify_all();
						r_cond.notify_all();
					}
					break;
					case WCB_FIN_WAIT_1:
					{
						state = WCB_CLOSING;
					}
					break;
					case WCB_FIN_WAIT_2:
					{
						timer_state = now;
						state = WCB_TIME_WAIT;
					}
					break;
					default:
					{
						state = WCB_CLOSED;
						wcb_errno = wawo::E_ECONNABORTED;
						reply_rst_to_address(so, inpack->header.ack, inpack->from);
					}
					break;
					}
					FINACK();
				}

				it = rcv_received.erase(it);
			}
		}

		if ( rb->count() == 0 && rb_standby->count() != 0 ) {
			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			WAWO_ASSERT( rb_standby->count() != 0 );
			if (rb->count() == 0) {
				rb.swap(rb_standby);

				WAWO_ASSERT(rb->count() != 0);
				if (r_flag&READ_RWND_LESS_THAN_MTU) {
					r_flag |= READ_RWND_MORE_THAN_MTU;
					rcv_info.wnd = rb_standby->left_capacity();
				}

				if (!(wcb_option&WCP_O_NONBLOCK)) {
					r_cond.notify_all();
				}
			}
		}

		if (!(wcb_option&WCP_O_NONBLOCK)) {
			if (r_flag&(READ_RECV_ERROR|READ_LOCAL_READ_SHUTDOWNED))
			{
				lock_guard<spin_mutex> lg_state(mutex);
				cond.notify_all();

				lock_guard<spin_mutex> lg_r_mutex(r_mutex);
				r_cond.notify_all();
			}
		}
	}

	void WCB::check_flights(u64_t const& now) {

//#define DISABLE_RTO
#ifdef DISABLE_RTO
		return;
#endif

		if ( !(wcb_flag&(SND_UNA_UPDATE|SND_FAST_RECOVERED|SND_BIGGEST_SACK_UPDATE)) && (now)<snd_last_rto_timer+WCP_RTO_CLOCK_GRANULARITY) {
			return;
		}
		snd_last_rto_timer = now;

		u32_t lost_count = 0;
		u32_t max_sent_time = 0;

		WCB_PackList::iterator&& it = snd_flights.begin();
		while (it != snd_flights.end()) {

			WWSP<WCB_pack> const& flight_pack = *it;
			if ((wcb_flag&SND_UNA_UPDATE) && flight_pack->header.seq < snd_info.una) {
				_handle_pack_acked(it,now);
				it = snd_flights.erase(it);
				continue;
			}

			static const char* retransmit_reason[] = {
				"",
				"AS", //ack skip
				"FR", //fast recovery
				"RO", //rto
			};

			(void)retransmit_reason;

			u8_t retransmit = 0;
			u32_t timediff = static_cast<u32_t>(now - flight_pack->sent_ts);
			i32_t skip = snd_biggest_ack - flight_pack->header.seq;

			if( (skip>=1) && timediff >= (srtt + (rttvar>>2) )) {
				retransmit = 1;
			}
			else if ((wcb_flag&SND_FAST_RECOVERED) && timediff >= (srtt + (rttvar >> 2))) {
				retransmit = 2;
			}
			else if (timediff >= static_cast<u32_t>(rto)) {
				++lost_count;
				retransmit = 3;

				if ( !(wcb_flag&SND_LOST_DETECTED) && (flight_pack->header.seq>snd_lost_biggest_seq)) {
					snd_lost_biggest_seq = flight_pack->header.seq;
				}

				if (flight_pack->sent_times > max_sent_time) {
					max_sent_time = flight_pack->sent_times;
				}
			}
			else {}

			if (retransmit > 0) {
				int sndrt = send_pack(*it);

				if (sndrt != wawo::OK) {
					if (sndrt != wawo::E_SOCKET_SEND_BLOCK) {
						lock_guard<spin_mutex> lg_s_mutex(s_mutex);
						wcb_errno = sndrt;
						s_flag |= WRITE_SEND_ERROR;

						WAWO_ERR("[wcp][rt][%s]skip: %d,seq: %u ,failed: %d,stimes: %u,una: %u,cwnd: %u,rwnd: %u,nflight: %u,ssthresh: %u,rto: %u,srtt: %u,rttvar: %u",
							retransmit_reason[retransmit], skip, flight_pack->header.seq, sndrt, flight_pack->sent_times, snd_info.una, snd_info.cwnd, snd_info.rwnd, snd_nflight_bytes, snd_info.ssthresh, rto, srtt, rttvar
						);
					}

					WCP_TRACE("[wcp][rt][%s]skip: %d,seq: %u,failed: %d,stimes: %u,una: %u,cwnd: %u,rwnd: %u,nflight: %u,ssthresh: %u,rto: %u,srtt: %u,rttvar: %u",
						retransmit_reason[retransmit], skip, flight_pack->header.seq, sndrt, flight_pack->sent_times, snd_info.una, snd_info.cwnd, snd_info.rwnd, snd_nflight_bytes, snd_info.ssthresh, rto, srtt, rttvar
					);
					break;
				}

				flight_pack->sent_ts = now;
				flight_pack->sent_times++;

				WCP_TRACE("[wcp][rt][%s]td: %u,skip: %d,seq: %u,stimes: %u,una: %u,cwnd: %u,rwnd: %u,nflight: %u,ssthresh: %u,rto: %u,srtt: %u,rttvar: %u",
					retransmit_reason[retransmit], timediff, skip, flight_pack->header.seq, flight_pack->sent_times, snd_info.una, snd_info.cwnd, snd_info.rwnd, snd_nflight_bytes, snd_info.ssthresh, rto, srtt, rttvar
				);
			}

			++it;
		}

		wcb_flag &= ~(SND_BIGGEST_SACK_UPDATE |SND_UNA_UPDATE | SND_FAST_RECOVERED);

		if ( lost_count>=WCP_LOST_THRESHOLD || max_sent_time >= 5 ) {

			WAWO_ASSERT( rto >= WCP_RTO_MIN ) ;
			rto = WAWO_MIN((rto >> 1) + rto, WCP_RTO_MAX);
			WCP_TRACE("[wcp]lost detected, wcp new rto: %u, srrt: %u, rttvar: %u", rto, srtt, rttvar);

			if ( (state == WCB_ESTABLISHED) && (now-snd_lost_detected_timer)>= WAWO_MAX(srtt,30) ) {
				snd_lost_detected_timer = now;
				snd_lost_bigger_than_lost_seq_time = 0;

				u32_t last_flight_max = snd_nflight_bytes_max;
				(void)last_flight_max;

				u32_t half_flight_max = snd_nflight_bytes_max >> 1;
				u32_t new_ssthresh = WAWO_MAX(half_flight_max,  WCP_SND_SSTHRESH_MIN );
				WAWO_ASSERT( snd_nflight_bytes_max >= snd_nflight_bytes );

				snd_lost_cwnd_for_fast_recovery_compensation = (new_ssthresh-(snd_nflight_bytes>>1));
				snd_info.ssthresh = WAWO_MIN(new_ssthresh, snd_info.ssthresh );

				snd_nflight_bytes_max = WAWO_MAX(snd_nflight_bytes_max >>1, snd_nflight_bytes);
				wcb_flag |= (SND_CWND_SLOW_START|SND_LOST_DETECTED);
				wcb_flag &= ~SND_CWND_CONGEST_AVOIDANCE;

				snd_info.cwnd = WCP_CWND_Cfg::WCP_LW*WCP_MTU;
				WCP_TRACE("[wcp]lost detected in establish state,ssthresh: %u,una: %u, rwnd: %u,last_flight_max: %u,curr_flight_max: %u,nflight: %u,compensation: %u",
					snd_info.ssthresh, snd_info.una, snd_info.rwnd, last_flight_max, snd_nflight_bytes_max, snd_nflight_bytes, snd_lost_cwnd_for_fast_recovery_compensation);
			}
		}
	}


	//@issue 1: we did not considerate faireness for each session for current implementation
	void WCB::check_send(u64_t const& now) {

		{
			//update rwnd manually
			if ( (r_flag&READ_RWND_MORE_THAN_MTU) && (now - r_timer_last_rwnd_update)>WCP_RWND_CHECK_GRANULARITY) {

				if (s_sending_ignore_seq_space.size() == 0) {
					update_rwnd();
				}
				else {
					WWSP<WCB_pack>& pack = s_sending_ignore_seq_space.front();
					pack->header.flag |= WCP_FLAG_WND;
				}
				r_timer_last_rwnd_update = now;
			}

			while (s_sending_ignore_seq_space.size()) {

				WWSP<WCB_pack>& pack = s_sending_ignore_seq_space.front();
				int sndrt = send_pack(pack);
				if (sndrt != wawo::OK) {
					if (sndrt != wawo::E_SOCKET_SEND_BLOCK) {
						lock_guard<spin_mutex> lg_s_mutex(s_mutex);
						wcb_errno = sndrt;
						s_flag |= WRITE_SEND_ERROR;
					}
					return;
				}
				s_sending_ignore_seq_space.pop();
			}

			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			if (s_flag&WRITE_SEND_ERROR) {
				WAWO_WARN("[wcp][%u]WCB::check_send, send error already: %s", fd, remote_addr.address_info().cstr);
				return;
			}

			while ( s_sending_standby.size() ) {
				snd_sending.push(s_sending_standby.front());
				s_sending_standby.pop();
			}

			//split sending buffer into segment if allowed
			while ( !(s_flag&WRITE_LOCAL_FIN_SENT) && (snd_sending.size() ==0) && (snd_info.cwnd- snd_nflight_bytes)>= WCP_MTU && (sb->count()>0) ) {
				WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
				opack->header.seq = snd_info.dsn++;
				opack->header.flag = WCP_FLAG_DAT | WCP_FLAG_ACK;

				WWSP<wawo::packet> data = wawo::make_shared<wawo::packet>(WCP_MDU);
				u32_t nread = sb->read(data->begin(), WCP_MDU);
				data->forward_write_index(nread);
				opack->data = data;
				opack->header.dlen = nread&0xFFFF;

				snd_sending.push(opack);
			}
		}

		while ( snd_sending.size()) {
			WWSP<WCB_pack>& pack = snd_sending.front();

			u32_t ntotal_bytes = pack->header.dlen + WCP_HeaderLen;
			if ( ((ntotal_bytes + snd_nflight_bytes) >= WAWO_MIN(snd_info.cwnd, snd_info.rwnd)) )
			{
				WCP_TRACE("[wcp]congest seq: %u,flag: %u,size: %u,una: %u,cwnd: %u,rwnd: %u,flight: %u,ssthresh: %u,rto: %u,srtt: %u,rttvar: %u",
					pack->header.seq, pack->header.flag, ntotal_bytes,snd_info.una, snd_info.cwnd, snd_info.rwnd, snd_nflight_bytes, snd_info.ssthresh, rto,srtt,rttvar
				);
				break;
			}

			WAWO_ASSERT(pack->header.seq == snd_info.next, "[wcp][%d]seq: %u, una: %u, flag: %u, next: %u", fd, pack->header.seq, snd_info.una, pack->header.flag, snd_info.next );
			WAWO_ASSERT(pack->header.seq >= snd_info.una, "[wcp][%d]seq: %u, una: %u, flag: %u",fd, pack->header.seq, snd_info.una, pack->header.flag );

			int sndrt = send_pack(pack);
			if (sndrt != wawo::OK) {
				if (sndrt != wawo::E_SOCKET_SEND_BLOCK) {
					lock_guard<spin_mutex> lg_s_mutex(s_mutex);
					wcb_errno = sndrt;
					s_flag |= WRITE_SEND_ERROR;
				}
				break;
			}

			WAWO_ASSERT(!WCPPACK_TEST_FLAG(*pack, (WCP_FLAG_SACK | WCP_FLAG_KEEP_ALIVE | WCP_FLAG_KEEP_ALIVE_REPLY)));

			pack->sent_ts = now;
			pack->sent_times = 1;

			snd_flights.push_back(pack);
			snd_nflight_bytes += ntotal_bytes;

			if ( snd_nflight_bytes > snd_nflight_bytes_max) {
				snd_nflight_bytes_max = snd_nflight_bytes;
			}

			snd_info.next++;

			if (WCPPACK_TEST_FLAG(*pack, WCP_FLAG_FIN)) {
				lock_guard<spin_mutex> lg_wcp_state(mutex);

				switch (state) {
				case WCB_ESTABLISHED:
				{
					timer_state = now;
					state = WCB_FIN_WAIT_1;

					lock_guard<spin_mutex> lg_s_mutex( s_mutex );
					s_flag |= WRITE_LOCAL_FIN_SENT;
				}
				break;
				case WCB_CLOSE_WAIT:
				{
					state = WCB_LAST_ACK;
					timer_state = now;
				}
				break;
				default:
				{
					WAWO_ASSERT(!"wcp state logic issue");
				}
				break;
				}
				break;
			}

			if (WCPPACK_TEST_FLAG(*pack, WCP_FLAG_SYN)) {
				lock_guard<spin_mutex> lg_wcp_state(mutex);
				WAWO_ASSERT(state == WCB_SYNING || state == WCB_SYN_RECEIVED);
				if (state == WCB_SYNING) {
					state = WCB_SYN_SENT;
				}
			}

			snd_sending.pop();
		}
	}

	int WCB::send_pack(WWSP<WCB_pack> const& pack) {
		WAWO_ASSERT(!remote_addr.is_null());
		pack->header.ack = rcv_info.next;
		pack->header.wnd = rcv_info.wnd;
		return inject_to_address(so, pack, remote_addr);
	}

	void WCB::listen_handle_syn() {

		WAWO_ASSERT(wcb_flag&WCB_FLAG_IS_LISTENER);

		lock_guard<spin_mutex> lg(received_vec_mutex);
		u32_t received_size = received_vec_standby->size();
		u32_t i = 0;
		for( ;i<received_size;++i ) {

			WWSP<WCB_received_pack> const& pack = (*received_vec_standby)[i];
			address const& from = pack->from;

			{
				lock_guard<spin_mutex> lg_r_mutex(r_mutex);
				if ((backlogq.size() + backloglist_pending.size()) == backlog_size) {
					reply_rst_to_address( so, pack->header.ack, from );
					continue;
				}
			}

			//according to RFC 793, if state in LISTEN, just ignore RST
			if (WCPPACK_TEST_FLAG( *pack, WCP_FLAG_RST)) {
				WAWO_ASSERT(state == WCB_LISTEN);
				WAWO_WARN("[wcp]WCB::accept, flag none WCP_FLAG_SYN, flag: %u, ignore, remote addr: %s, reply rst", pack->header.flag, from.address_info().cstr);
				continue;
			}

			if (!WCPPACK_TEST_FLAG( *pack, WCP_FLAG_SYN)) {
				WAWO_WARN("[wcp]WCB::accept, flag none WCP_FLAG_SYN, flag: %u, remote addr: %s, reply rst", pack->header.flag, from.address_info().cstr);
				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			WAWO_DEBUG("[wcp]WCB::accept, receive new syn from: %s", from.address_info().cstr);
			WWRP<socket> accepted_so = wawo::make_ref<socket>(so->get_buffer_cfg(), F_PF_INET, ST_DGRAM, P_UDP, OPTION_NONE);

			int openrt = accepted_so->open();
			if (openrt != wawo::OK) {
				WAWO_ERR("[wcp]WCB::accept, call socket() failed: %d", openrt);
				accepted_so->close(openrt);

				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			int reusert = accepted_so->reuse_addr();
			if(reusert != wawo::OK) {
				WAWO_ERR("[wcp]WCB::accept, call reuse_addr() failed: %d", reusert);

				accepted_so->close(reusert);
				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			reusert = accepted_so->reuse_port();
			if(reusert != wawo::OK) {
				WAWO_ERR("[wcp]WCB::accept, call reuse_port() failed: %d", reusert);
				accepted_so->close(reusert);

				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			sockaddr_in addr_local;
			addr_local.sin_family = AF_INET;
			addr_local.sin_addr.s_addr = local_addr.get_netsequence_ulongip();

#if WAWO_ISGNU
			addr_local.sin_port = local_addr.get_netsequence_port();
#else
			addr_local.sin_port = 0;
#endif
			wawo::net::address bind_addr(addr_local);

			int bindrt = accepted_so->bind(bind_addr);
			if (bindrt != wawo::OK ) {
				WAWO_ERR("[wcp]WCB::accept, call bind failed: %d", bindrt);
				accepted_so->close(bindrt);

				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			int connrt = accepted_so->connect(from);
			if (connrt != wawo::OK ) {
				WAWO_ERR("[wcp]WCB::accept, call connect failed: %d", connrt);
				accepted_so->close(connrt);

				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			int nonblocking = accepted_so->turnon_nonblocking();
			if (nonblocking != wawo::OK) {
				WAWO_ERR("[wcp]WCB::accept, turn on nonblocking failed: %d", nonblocking);
				accepted_so->close(nonblocking);

				reply_rst_to_address(so, pack->header.ack, from);
				continue;
			}

			wawo::u32_t _four_tuple_hash_id = four_tuple_hash( local_addr, from );

			WWRP<WCB> wcb = wawo::make_ref<WCB>();
			wcb->init();
			wcb->wcb_flag |= WCB_FLAG_IS_PASSIVE_OPEN;
			wcb->four_tuple_hash_id = _four_tuple_hash_id;
			wcb->local_addr = accepted_so->get_local_addr();
			wcb->remote_addr = from;
			wcb->so = accepted_so;
			wcb->fd = wcp::make_wcb_fd();
			wcb->set_rcv_buffer_size( get_rcv_buffer_size() );
			wcb->set_snd_buffer_size( get_snd_buffer_size() );

			wcb->SYN_RCVD(pack);

			if ( wcp::instance()->add_to_four_tuple_hash_map(wcb)<0 ) {
				WAWO_WARN("[wcp]WCB::accept, add_to_four_tuple_hash_map failed, remote addr: %s, reply rst", from.address_info().cstr);
				accepted_so->close();
				reply_rst_to_address(so, pack->header.ack, wcb->remote_addr );
				continue;
			}

			wcp::instance()->watch(wcb);

			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			backloglist_pending.push_back(wcb);
		}

		received_vec_standby->erase( received_vec_standby->begin() , received_vec_standby->begin() + i);
	}

	void WCB::pump_packs() {

		lock_guard<spin_mutex> lg_received_vec(received_vec_mutex);

		int recvfrom_ec;
		do {

			WAWO_ASSERT(so != NULL);
			byte_t _buffer[WCP_MTU];

			address from;
			wawo::u32_t nbytes = so->recvfrom(_buffer, WCP_MTU, from, recvfrom_ec);

			if (recvfrom_ec != wawo::OK) {
				if (recvfrom_ec != wawo::E_SOCKET_RECV_BLOCK) {
					wcb_errno = recvfrom_ec;
					lock_guard<spin_mutex> lg_r_mutex(r_mutex);
					r_flag |= READ_RECV_ERROR;
				}
				break;
			}

			WAWO_ASSERT(nbytes >= WCP_HeaderLen);
			WWSP<WCB_received_pack> inpack = wawo::make_shared<WCB_received_pack>();
			WCP_RECEIVED_PACK_FROM_UDPMESSAGE(*inpack, _buffer, nbytes);
			inpack->from = from;
			received_vec_standby->push_back( inpack );

#ifdef WCP_TRACE_INOUT_PACK
			WCP_TRACE("[wcp]WCB::pump_packs, recvfrom: %s, seq: %u, flag: %u, ack: %u, wnd: %u",
				from.address_info().cstr, inpack->header.seq, inpack->header.flag, inpack->header.ack, inpack->header.wnd );
#endif
		} while (recvfrom_ec == wawo::OK);

	}

	int WCB::recv_pack(WWSP<WCB_received_pack>& pack, address& from) {

		int ec;
		byte_t _buffer[WCP_MTU];
		wawo::u32_t nbytes = so->recvfrom(_buffer, WCP_MTU, from, ec);
		WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);

		WAWO_ASSERT(nbytes >= WCP_HeaderLen);
		WWSP<WCB_received_pack> inpack = wawo::make_shared<WCB_received_pack>();
		WCP_RECEIVED_PACK_FROM_UDPMESSAGE(*inpack, _buffer, nbytes);
		inpack->from = from;
		pack = inpack;

		WAWO_DEBUG("[wcp]WCB::recv_pack, from: %s, remote: %s", from.address_info().cstr, remote_addr.address_info().cstr);
		return wawo::OK;
	}


	int WCB::connect(wawo::net::address const& addr) {
		lock_guard<spin_mutex> lg(mutex);
		WAWO_ASSERT(remote_addr.is_null());
		remote_addr = addr;
		WAWO_ASSERT(state == WCB_CLOSED);

		{
			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			SYN();
		}
		state = WCB_SYNING;
		timer_state = wawo::time::curr_milliseconds();

		if ((wcb_option&WCP_O_NONBLOCK)) {
			wawo::set_last_errno(WAWO_NEGATIVE(EINPROGRESS));
			return WAWO_NEGATIVE(EINPROGRESS);
		}

		while (wcb_errno == 0 && state != WCB_ESTABLISHED) {
			cond.wait<spin_mutex>(mutex);
		}
		WAWO_ASSERT(state == WCB_ESTABLISHED ? wcb_errno == 0 : true);

		return wcb_errno;
	}

	int WCB::shutdown(int const& shutdown_flag) {
		lock_guard<spin_mutex> lg(mutex);

		WAWO_ASSERT(shutdown_flag == WCB_SHUT_RD
		|| shutdown_flag == WCB_SHUT_WR
		|| shutdown_flag == WCB_SHUT_RDWR
		);

		int ec = wawo::E_ENOTCONN;
		if (shutdown_flag==WCB_SHUT_WR||shutdown_flag == WCB_SHUT_RDWR) {
			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			if ( !(s_flag&WRITE_LOCAL_WRITE_SHUTDOWNED) ) {
				ec = wawo::OK;
				FIN();
				s_flag |= WRITE_LOCAL_WRITE_SHUTDOWNED;
			}
		}

		if (shutdown_flag==WCB_SHUT_RD || shutdown_flag == WCB_SHUT_RDWR) {
			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			if (!(r_flag&READ_LOCAL_READ_SHUTDOWNED)) {
				ec = wawo::OK;
				r_flag |= READ_LOCAL_READ_SHUTDOWNED;;
				r_cond.notify_all();
			}
		}

		if (ec!=wawo::OK) {
			wawo::set_last_errno(ec);
			return -1;
		}

		return wawo::OK;
	}


	int WCB::close() {
		lock_guard<spin_mutex> lg(mutex);
		if ( wcb_flag&WCB_FLAG_CLOSED_CALLED ) {
			wawo::set_last_errno(wawo::E_EALREADY);
			return wawo::E_EALREADY;
		}

		wcb_flag |= WCB_FLAG_CLOSED_CALLED;

		if (state == WCB_LISTEN) {
			state = WCB_CLOSED;
		} 
		
		if (state == WCB_SYN_SENT || state == WCB_SYNING) {
			state = WCB_CLOSED;
			if (wcb_errno == wawo::OK) {
				wcb_errno = wawo::E_ECONNABORTED;
			}
		}

		if (state == WCB_CLOSED) {
			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			s_flag |= WRITE_LOCAL_WRITE_SHUTDOWNED;

			lock_guard<spin_mutex> lg_r_mutex( r_mutex );
			r_flag |= READ_LOCAL_READ_SHUTDOWNED;

			r_cond.notify_all();
			return wawo::OK;
		}

		if (state == WCB_SYN_RECEIVED ||
			state == WCB_ESTABLISHED
			) {

			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			if ( !(s_flag&WRITE_LOCAL_WRITE_SHUTDOWNED)) {
				FIN();
				s_flag |= WRITE_LOCAL_WRITE_SHUTDOWNED;
			}

			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			if ( !(r_flag&READ_LOCAL_READ_SHUTDOWNED) ) {
				r_flag |= READ_LOCAL_READ_SHUTDOWNED;
			}
			r_cond.notify_all();
			return wawo::OK;
		}

		if (state == WCB_CLOSE_WAIT) {
			lock_guard<spin_mutex> lg_s_mutex(s_mutex);
			if ( !(s_flag&WRITE_LOCAL_WRITE_SHUTDOWNED)) {
				FIN();
				s_flag |= WRITE_LOCAL_WRITE_SHUTDOWNED;
			}

			lock_guard<spin_mutex> lg_r_mutex(r_mutex);
			if ( !(r_flag&READ_LOCAL_READ_SHUTDOWNED)) {
				r_flag |= READ_LOCAL_READ_SHUTDOWNED;
			}
			r_cond.notify_all();
			return wawo::OK;
		}

		WAWO_THROW( "wcp state logic issue" );
	}

	int WCB::listen(int const& backlog ) {
		lock_guard<spin_mutex> lg(mutex);
		if (local_addr.is_null()) {
			wawo::set_last_errno(E_EOPNOTSUPP);
			return wawo::E_EOPNOTSUPP;
		}

		WAWO_ASSERT(state == WCB_CLOSED);
		state = WCB_LISTEN;
		backlog_size = backlog;
		wcb_flag |= WCB_FLAG_IS_LISTENER;

		return wawo::OK;
	}

	wcp::wcp() :
		m_state (S_IDLE)
	{
		//int startrt = start();
		//WAWO_ASSERT(startrt == wawo::OK);
	}

	wcp::~wcp() { WAWO_ASSERT(m_state == S_IDLE || m_state == S_EXIT); }

	void wcp::on_start() {
		lock_guard<shared_mutex> lg(m_mutex);
		m_state = S_RUN;

		m_self_ticker = wawo::make_shared<wawo::thread::fn_ticker>(std::bind(&wcp::run, this) );
		observer_ticker::instance()->schedule(m_self_ticker);
	}

	void wcp::on_stop() {

		if (m_self_ticker != NULL) {
			observer_ticker::instance()->deschedule(m_self_ticker);
		}

		lock_guard<shared_mutex> lg(m_mutex);
		m_state = S_EXIT;

		m_self_ticker = NULL;

		{
			lock_guard<shared_mutex> _(m_wcb_map_mutex);
			WCBMap::iterator it = m_wcb_map.begin();
			while (it != m_wcb_map.end()) {
				it->second->close();
				it->second->so->close(wawo::E_SOCKET_FORCE_CLOSE);
				++it;
			}

			m_wcb_map.clear();
			m_wcb_four_tuple_map.clear();
		}

		m_wpoll_map.clear();
	}


	void wcp::_execute_ops() {

		while (!m_ops.empty()) {

			lock_guard<shared_mutex> lg(m_wcb_map_mutex);
			Op op = { NULL,OP_NONE };
			_pop_op(op);

			if (op.op == OP_NONE) {
				break;
			}

			WAWO_ASSERT(op.wcb != NULL);
			WAWO_ASSERT(op.wcb->so != NULL);
			WAWO_ASSERT(op.wcb->so->is_nonblocking());

#ifdef DEBUG
			WCBMap::iterator it = m_wcb_map.find( op.wcb->fd );
			WAWO_ASSERT( it == m_wcb_map.end() );
#endif
			switch (op.op) {
			case OP_WATCH:
			{
				m_wcb_map.insert(WCBPair(op.wcb->fd, op.wcb));
				WAWO_ASSERT(op.wcb != NULL);
				op.wcb->so->begin_async_read(WATCH_OPTION_INFINITE,op.wcb, wawo::net::wcb_pump_packs, wawo::net::wcb_socket_error);
			}
			break;
			case OP_NONE:
			{}
			break;
			}
		}
	}

	void wcp::_update() {

		std::vector< WWRP<WCB> > wcb_to_delete;
		{
			shared_lock_guard<shared_mutex> lg_map(m_wcb_map_mutex);
			WCBMap::iterator it = m_wcb_map.begin();
			while (it != m_wcb_map.end()) {
				u64_t now = wawo::time::curr_milliseconds();
				WCB_State s = it->second->update(now);
				if (s == WCB_RECYCLE) {
					WWRP<wawo::net::socket> const& so = it->second->so;
					WAWO_ASSERT( so != NULL );

					so->close( it->second->wcb_errno );
					wcb_to_delete.push_back(it->second);
				}
				++it;
			}
		}

		if (wcb_to_delete.size()) {
			lock_guard<shared_mutex> lg_map(m_wcb_map_mutex);
			for (u32_t i = 0; i < wcb_to_delete.size(); i++) {

				if (wcb_to_delete[i]->wcb_flag&WCB_FLAG_IS_PASSIVE_OPEN) {
					remove_from_four_tuple_hash_map(wcb_to_delete[i]);
				}
				m_wcb_map.erase(wcb_to_delete[i]->fd);
			}
		}
	}

	std::atomic<int> wcp::s_wcb_auto_increament_id(1);

	int wcp::socket(int const& family, int const& socket_type, int const& protocol) {

		WAWO_ASSERT(family == AF_INET);
		WAWO_ASSERT(socket_type == SOCK_DGRAM);
		WAWO_ASSERT(protocol == IPPROTO_UDP);

		WWRP<wawo::net::socket> udpsocket = wawo::make_ref<wawo::net::socket>(F_AF_INET, ST_DGRAM, P_UDP);
		int openrt = udpsocket->open();
		WAWO_RETURN_V_IF_NOT_MATCH( WAWO_NEGATIVE(socket_get_last_errno()) , openrt == wawo::OK);

		WWRP<WCB> wcb = wawo::make_ref<WCB>();
		wcb->init();
		wcb->so = udpsocket;
		wcb->fd = wcp::make_wcb_fd();

		lock_guard<spin_mutex> slg(m_wcb_create_pending_map_mutex);
		const WCBMap::iterator& it = m_wcb_create_pending_map.find(wcb->fd);
		if (it != m_wcb_create_pending_map.end()) {
			wawo::set_last_errno(wawo::E_WCP_INTERNAL_ERROR);
			return wawo::E_WCP_INTERNAL_ERROR;
		}
		m_wcb_create_pending_map.insert( WCBPair(wcb->fd, wcb) );
		return wcb->fd;
	}

	int wcp::connect(int const& fd, const struct sockaddr* addr, socklen_t const& len ) {
		(void)len;

		lock_guard<spin_mutex> slg(m_wcb_create_pending_map_mutex);
		const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
		if (it == m_wcb_create_pending_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWRP<WCB> wcb = it->second;
		WAWO_ASSERT(wcb != NULL);

		int nonblocking = wcb->so->turnon_nonblocking();
		WAWO_RETURN_V_IF_NOT_MATCH( nonblocking, nonblocking == wawo::OK );
		m_wcb_create_pending_map.erase(it);
		{
			lock_guard<shared_mutex> lg_wcb_map_mutex(m_wcb_map_mutex);
			m_wcb_map.insert(WCBPair(fd, wcb));
		}

		wcb->so->begin_async_read(WATCH_OPTION_INFINITE, wcb, wawo::net::wcb_pump_packs, wawo::net::wcb_socket_error);
		return wcb->connect(wawo::net::address(*((sockaddr_in*)addr)));
	}

	int wcp::bind(int const& fd, const struct sockaddr* addr, socklen_t const& len ) {
		(void)len;

		lock_guard<spin_mutex> slg(m_wcb_create_pending_map_mutex);
		const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
		if (it == m_wcb_create_pending_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWRP<WCB>& wcb = it->second;
		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		wcb->local_addr = wawo::net::address(*((sockaddr_in*)addr));
		int bindrt = wcb->so->bind( wcb->local_addr );

		if (bindrt != wawo::OK) {
			wcb->wcb_errno = WAWO_NEGATIVE(wawo::socket_get_last_errno());
			return wcb->wcb_errno;
		}

		WAWO_ASSERT( wcb != NULL );
		return wawo::OK;
	}

	int wcp::listen(int const& fd, int const& backlog) {

		lock_guard<spin_mutex> slg(m_wcb_create_pending_map_mutex);
		const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
		if (it == m_wcb_create_pending_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWRP<WCB> wcb = it->second;
		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		int nonblocking = wcb->so->turnon_nonblocking();
		WAWO_RETURN_V_IF_NOT_MATCH(nonblocking, nonblocking == wawo::OK);

		int udplistening = wcb->so->listen(backlog);
		WAWO_RETURN_V_IF_NOT_MATCH(nonblocking, udplistening == wawo::OK);

		int listenrt = wcb->listen(backlog);
		WAWO_RETURN_V_IF_NOT_MATCH( listenrt, listenrt==wawo::OK );

		m_wcb_create_pending_map.erase(it);

		lock_guard<shared_mutex> lg_wcb_map_mutex(m_wcb_map_mutex);
		m_wcb_map.insert(WCBPair(fd, wcb));
		wcb->so->begin_async_read(WATCH_OPTION_INFINITE,wcb, wawo::net::wcb_pump_packs, wawo::net::wcb_socket_error);

		return wawo::OK;
	}

	int wcp::accept(int const& fd, struct sockaddr* addr, socklen_t* addrlen ) {

		shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
		const WCBMap::iterator& it = m_wcb_map.find(fd);
		if (it == m_wcb_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWRP<WCB>& wcb = it->second;
		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		lock_guard<spin_mutex> lg_r_mutex(wcb->r_mutex);

		if (wcb->r_flag& (READ_RECV_ERROR|READ_LOCAL_READ_SHUTDOWNED)) {
			if (wcb->wcb_errno != 0) {
				wawo::set_last_errno( wcb->wcb_errno );
				return wcb->wcb_errno;
			}
			wawo::set_last_errno(wawo::E_ECONNABORTED);
			return wawo::E_ECONNABORTED;
		}

		if (!wcb->backlogq.size()) {
			wawo::set_last_errno(EWOULDBLOCK);
			return WAWO_NEGATIVE(EWOULDBLOCK);
		}

		WWRP<WCB> _wcb = wcb->backlogq.front();
		wcb->backlogq.pop();

		WAWO_ASSERT( !_wcb->remote_addr.is_null() );

		sockaddr_in* addr_in = (sockaddr_in*) addr;
		addr_in->sin_family = SOCK_DGRAM;
		addr_in->sin_port = _wcb->remote_addr.get_netsequence_port();
		addr_in->sin_addr.s_addr = _wcb->remote_addr.get_netsequence_ulongip();

		(void)addrlen;

		return _wcb->fd;
	}

	int wcp::shutdown(int const& fd, int const& flag) {
		shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
		const WCBMap::iterator& it = m_wcb_map.find(fd);
		if (it == m_wcb_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWRP<WCB>& wcb = it->second;
		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		if (wcb->wcb_flag&WCB_FLAG_IS_LISTENER) {
			wawo::set_last_errno(wawo::E_EOPNOTSUPP);
			return wawo::E_EOPNOTSUPP;
		}

		return wcb->shutdown(flag);
	}

	int wcp::close(int const& fd) {

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it != m_wcb_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			lock_guard<spin_mutex> lg_create_pending(m_wcb_create_pending_map_mutex);
			const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
			if (it != m_wcb_create_pending_map.end()) {
				wcb = it->second;
				m_wcb_create_pending_map.erase(it);
			}
		}

		if (wcb == NULL) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);
		return wcb->close();
	}

	int wcp::send(int const& fd, byte_t const* const buffer, u32_t const& len, int const& flag) {
		(void)flag;

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it == m_wcb_map.end()) {
				wawo::set_last_errno(wawo::E_EBADF);
				return wawo::E_EBADF;
			}
			wcb = it->second;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		lock_guard<spin_mutex> lg_wcb_s_mutex(wcb->s_mutex);
		if (wcb->s_flag& (WRITE_SEND_ERROR | WRITE_LOCAL_WRITE_SHUTDOWNED)) {
			if (wcb->wcb_errno != 0) {
				wawo::set_last_errno( wcb->wcb_errno );
				return wcb->wcb_errno;
			}
			wawo::set_last_errno(wawo::E_ECONNABORTED);
			return wawo::E_ECONNABORTED;
		}

		u32_t left_capacity = wcb->sb->left_capacity();
		if ( (left_capacity<len) && left_capacity<(2*WCP_MDU) ) {
			wawo::set_last_errno(EWOULDBLOCK);
			return WAWO_NEGATIVE(EWOULDBLOCK);
		}

		return wcb->sb->write(buffer, len);
	}

	int wcp::recv(int const&fd, byte_t* const buffer_o, u32_t const& size, int const& flag ) {
		(void)flag;

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it == m_wcb_map.end()) {
				wawo::set_last_errno(wawo::E_EBADF);
				return wawo::E_EBADF;
			}
			wcb = it->second;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		lock_guard<spin_mutex> lg_rb_recv(wcb->r_mutex);
	read_begin:
		if (wcb->r_flag&READ_LOCAL_READ_SHUTDOWNED) {
			wawo::set_last_errno(wawo::E_ECONNABORTED);
			return wawo::E_ECONNABORTED;
		}

		if (wcb->rb->count()>0) {
			u32_t nbytes = wcb->rb->read( buffer_o, size );

			WAWO_ASSERT( nbytes>0 );
			//WAWO_INFO("[wcp][%d:%s] read bytes: %u", wcb->fd, wcb->remote_addr.address_info().cstr, nbytes);
			return nbytes;
		}

		WAWO_ASSERT( wcb->rb->count() == 0 );

		if ( wcb->r_flag&READ_REMOTE_FIN_RECEIVED ) {
			if (wcb->r_flag&READ_REMOTE_FIN_READ_BY_USER) {
				wawo::set_last_errno(wawo::E_ECONNRESET);
				return wawo::E_ECONNRESET;
			}
			wcb->r_flag |= READ_REMOTE_FIN_READ_BY_USER;
			wawo::set_last_errno(wawo::OK);
			return wawo::OK;
		}

		if (wcb->r_flag&READ_RECV_ERROR) {
			WAWO_ASSERT(wcb->wcb_errno != 0);
			wawo::set_last_errno(wcb->wcb_errno);
			return wcb->wcb_errno;
		}

		//@NOTICE, 2018.01.10
		WAWO_ASSERT(wcb->wcb_errno == 0);

		//if (wcb->wcb_errno != 0) {
		//	wawo::set_last_errno(wcb->wcb_errno);
		//	return WAWO_NEGATIVE(wcb->wcb_errno);
		//}

		if (wcb->wcb_option&WCP_O_NONBLOCK) {
			wawo::set_last_errno(EWOULDBLOCK);
			return WAWO_NEGATIVE(EWOULDBLOCK);
		}

		WAWO_ASSERT( !(wcb->wcb_option&WCP_O_NONBLOCK));

		wcb->r_cond.wait<spin_mutex>(wcb->r_mutex);
		goto read_begin;
	}

	int wcp::getsockname(int const& fd, struct sockaddr* addr, socklen_t* addrlen ) {
		(void)addrlen;

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it == m_wcb_map.end()) {
				wawo::set_last_errno(wawo::E_EBADF);
				return wawo::E_EBADF;
			}
			wcb = it->second;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		sockaddr_in* addr_in = (sockaddr_in*)addr;
		addr_in->sin_family = SOCK_DGRAM;
		addr_in->sin_port = wcb->local_addr.get_netsequence_port();
		addr_in->sin_addr.s_addr = wcb->local_addr.get_netsequence_ulongip();

		return wawo::OK;
	}

	int wcp::getsockopt(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_len ) {

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it != m_wcb_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			lock_guard<spin_mutex> lg_create_pending(m_wcb_create_pending_map_mutex);
			const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
			if (it != m_wcb_create_pending_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		if (option_name == SO_REUSEADDR
#if WAWO_ISGNU
			|| option_name == SO_REUSEPORT
#endif
			) {
			return wcb->so->getsockopt(level, option_name, value, option_len);
		}

		if (option_name == SO_ERROR) {
			int& _v = (*(int*)value);
			_v = wcb->wcb_errno;
			return wawo::OK;
		}

		if(option_name == SO_RCVBUF) {
			int& _v = (*(int*)value);
			_v = wcb->get_rcv_buffer_size();
			return wawo::OK;
		}

		if(option_name == SO_SNDBUF) {
			int& _v = (*(int*)value);
			_v = wcb->get_snd_buffer_size();
			return wawo::OK;
		}


		(void)option_len;
		(void)level;
		WAWO_ASSERT( !"TODO" );
	}

	int wcp::setsockopt(int const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_len ) {

		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it != m_wcb_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			lock_guard<spin_mutex> lg_create_pending(m_wcb_create_pending_map_mutex);
			const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
			if (it != m_wcb_create_pending_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		if (option_name == SO_REUSEADDR) {
			return wcb->so->reuse_addr();
		}
#if WAWO_ISGNU
		else if (option_name == SO_REUSEPORT) {
			return wcb->so->reuse_port();
		}
#endif

		if(option_name == SO_RCVBUF) {
			return wcb->set_rcv_buffer_size( *(int*)(value) );
		}

		if(option_name == SO_SNDBUF)
		{
			return wcb->set_snd_buffer_size( *(int*)(value) );
		}


		if(option_name == IP_TOS) {

			return wawo::OK;
		}

		(void)option_len;
		(void)level;
		WAWO_ASSERT(!"TODO");
	}

	int wcp::fcntl_getfl(int const& fd, int& flag) {
		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it != m_wcb_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			lock_guard<spin_mutex> lg_create_pending(m_wcb_create_pending_map_mutex);
			const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
			if (it != m_wcb_create_pending_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		lock_guard<spin_mutex> lg_wcb( wcb->mutex );
		flag = wcb->wcb_option;
		return wawo::OK;
	}

	int wcp::fcntl_setfl(int const& fd, int const& flag) {
		WWRP<WCB> wcb;
		{
			shared_lock_guard<shared_mutex> slg(m_wcb_map_mutex);
			const WCBMap::iterator& it = m_wcb_map.find(fd);
			if (it != m_wcb_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			lock_guard<spin_mutex> lg_create_pending(m_wcb_create_pending_map_mutex);
			const WCBMap::iterator& it = m_wcb_create_pending_map.find(fd);
			if (it != m_wcb_create_pending_map.end()) {
				wcb = it->second;
			}
		}

		if (wcb == NULL) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WAWO_ASSERT(wcb != NULL);
		WAWO_ASSERT(wcb->so != NULL);

		lock_guard<spin_mutex> lg_wcb(wcb->mutex);
		wcb->wcb_option =flag;
		return wawo::OK;
	}



	int wcp::wpoll_create() {
		int id = wawo::atomic_increment(&s_wpoll_auto_increament_id);

		lock_guard<shared_mutex> lg(m_mutex);
		if (m_state != S_RUN) {
			wawo::set_last_errno(wawo::E_INVALID_STATE);
			return wawo::E_INVALID_STATE;
		}

		WWSP<wpoll> _wpoll = wawo::make_shared<wpoll>();
		_wpoll->evts.reserve(100);
		m_wpoll_map.insert( WpollPair(id, _wpoll) );

		return id;
	}

	int wcp::wpoll_close(int const& wpoll_handle) {
		lock_guard<shared_mutex> lg(m_mutex);

		if (m_state != S_RUN) {
			wawo::set_last_errno(wawo::E_INVALID_STATE);
			return wawo::E_INVALID_STATE;
		}

		WpollMap::iterator it = m_wpoll_map.find(wpoll_handle);
		if (it == m_wpoll_map.end()) {
			wawo::set_last_errno( wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS);
			return wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS;
		}
		m_wpoll_map.erase(it);
		return wawo::OK;
	}

	int wcp::wpoll_ctl(int const& wpoll_handle, wpoll_op const& op, wpoll_event const& evt ) {

		WAWO_ASSERT(evt.fd > 0);
		WAWO_ASSERT(wpoll_handle>0);

		shared_lock_guard<shared_mutex> lg(m_mutex);
		if (m_state != S_RUN) {
			wawo::set_last_errno(wawo::E_INVALID_STATE);
			return wawo::E_INVALID_STATE;
		}

		WpollMap::iterator it = m_wpoll_map.find(wpoll_handle);
		if (it == m_wpoll_map.end()) {
			wawo::set_last_errno( wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS);
			return wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS;
		}

		shared_lock_guard<shared_mutex> lg_wcb_map(m_wcb_map_mutex);
		WCBMap::iterator wcb_it = m_wcb_map.find(evt.fd);
		if (wcb_it == m_wcb_map.end()) {
			wawo::set_last_errno(wawo::E_EBADF);
			return wawo::E_EBADF;
		}

		WWSP<wpoll>& wpoll = it->second;

		switch (op) {

		case WPOLL_CTL_ADD:
			{
				WAWO_ASSERT(evt.cookie != NULL);

				lock_guard<shared_mutex> lg_evts( wpoll->mutex );
				wpoll_watch_event_vector::iterator evt_it = std::find_if(wpoll->evts.begin(), wpoll->evts.end(), [&](WWSP<wpoll_watch_event> const& _evt) {
					return evt.fd == _evt->fd;
				});

				if (evt_it != wpoll->evts.end()) {
					WAWO_ASSERT((*evt_it)->cookie == evt.cookie);
					(*evt_it)->evts |= evt.evts;
					return wawo::OK;
				}

				WWSP<wpoll_watch_event> _evt = wawo::make_shared<wpoll_watch_event>();
				_evt->fd = evt.fd;
				_evt->evts = evt.evts;
				_evt->cookie = evt.cookie;
				_evt->wcb = wcb_it->second;

				wpoll->evts.push_back(_evt);
				return wawo::OK;
			}
			break;
		case WPOLL_CTL_DEL:
			{
				lock_guard<shared_mutex> lg_evts(wpoll->mutex);
				wpoll_watch_event_vector::iterator evt_it = std::find_if(wpoll->evts.begin(), wpoll->evts.end(), [&](WWSP<wpoll_watch_event> const& _evt) {
					return evt.fd == _evt->fd;
				});

				if (evt_it == wpoll->evts.end()) {
					wawo::set_last_errno(wawo::E_EBADF);
					return wawo::E_EBADF;
				}

				(*evt_it)->evts &= ~evt.evts;

				if ( (*evt_it)->evts == 0) {
					wpoll->evts.erase(evt_it);
				}
				return wawo::OK;
			}
			break;
		}

		wawo::set_last_errno(wawo::E_WCP_WPOLL_INVALID_OP);
		return wawo::E_WCP_WPOLL_INVALID_OP;
	}

	int wcp::wpoll_wait(int const& wpoll_handle, wpoll_event evts[], u32_t const& size) {

		shared_lock_guard<shared_mutex> lg(m_mutex);
		if (m_state != S_RUN) {
			wawo::set_last_errno(wawo::E_INVALID_STATE);
			return wawo::E_INVALID_STATE;
		}

		WpollMap::iterator it = m_wpoll_map.find(wpoll_handle);
		if (it == m_wpoll_map.end()) {
			wawo::set_last_errno(wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS);
			return wawo::E_WCP_WPOLL_HANDLE_NOT_EXISTS;
		}

		u32_t count = 0;
		while (m_event_pending_queue.size() && (count<size) ) {
			wpoll_event_pending const& pending = m_event_pending_queue.front();
			if (pending.watch_evt->evts != 0) {
				evts[count++] = pending.wpoll_evt;
			}
			m_event_pending_queue.pop();
		}

		if (count > 0) {
			return count;
		}

		WAWO_ASSERT( m_event_pending_queue.size() == 0 );

		WWSP<wpoll>& _wpoll = it->second;
		shared_lock_guard<shared_mutex> lg_wpoll(_wpoll->mutex);
		wpoll_watch_event_vector::iterator evt_it = _wpoll->evts.begin();

		while( evt_it != _wpoll->evts.end() ) {

			wpoll_watch_event& __evt = *(*evt_it);

			if (__evt.wcb->state == WCB_RECYCLE) {
				evt_it = _wpoll->evts.erase(evt_it);
				continue;
			}

			wpoll_event _evt = { __evt.fd, 0, __evt.cookie };

			if (__evt.evts & WPOLLIN) {
				lock_guard<spin_mutex> _lg_backlog(__evt.wcb->r_mutex);
				if (__evt.wcb->wcb_flag&WCB_FLAG_IS_LISTENER) {
					if (__evt.wcb->backlogq.size()) {
						_evt.evts |= WPOLLIN;
					}
				} else {
					if (__evt.wcb->rb->count() || ( (__evt.wcb->r_flag&READ_REMOTE_FIN_RECEIVED) && !(__evt.wcb->r_flag&READ_REMOTE_FIN_READ_BY_USER)) ) {
						_evt.evts |= WPOLLIN;
					}
				}

				if (__evt.wcb->r_flag& (READ_RECV_ERROR| READ_LOCAL_READ_SHUTDOWNED)) {
					WAWO_ASSERT(__evt.wcb->wcb_errno != wawo::OK);
					_evt.evts |= WPOLLERR;
				}
			}

			if (__evt.evts&WPOLLOUT) {
				WAWO_ASSERT( !(__evt.wcb->wcb_flag&WCB_FLAG_IS_LISTENER) );
				lock_guard<spin_mutex> lg_s_mutex(__evt.wcb->s_mutex);
				if (__evt.wcb->s_flag&(WRITE_LOCAL_WRITE_SHUTDOWNED | WRITE_SEND_ERROR)) {
					WAWO_ASSERT(__evt.wcb->wcb_errno != wawo::OK);
					_evt.evts |= WPOLLERR;

					__evt.evts &= ~WPOLLOUT;//unwatch out
				}
				else {
					if ( (__evt.wcb->state == WCB_ESTABLISHED) && __evt.wcb->sb->left_capacity()>(WCP_MDU*2) ) {
						_evt.evts |= WPOLLOUT;
					}
				}
			}

			if (__evt.wcb->wcb_errno != 0) {
				_evt.evts |= WPOLLERR;
			}

			if (_evt.evts != 0) {

				if (count < size) {
					evts[count++] = _evt;
				}
				else {
					WAWO_ASSERT(count == size);
					wpoll_event_pending pending_evt = { _evt, *evt_it };
					m_event_pending_queue.push(pending_evt);
				}
			}
			++evt_it;
		}

		return count;
	}

	std::atomic<int> wcp::s_wpoll_auto_increament_id(1);
}}
