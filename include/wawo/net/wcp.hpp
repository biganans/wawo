#ifndef WAWO_NET_WCP_HPP
#define WAWO_NET_WCP_HPP

#include <map>
#include <unordered_map>
#include <queue>
#include <list>

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>

#include <wawo/thread/mutex.hpp>
#include <wawo/thread/ticker.hpp>

#include <wawo/bytes_ringbuffer.hpp>
#include <wawo/packet.hpp>
#include <wawo/net/address.hpp>

#include <wawo/log/logger_manager.h>

//#define DEBUG_WCP
#ifdef DEBUG_WCP
#	define WCP_TRACE WAWO_INFO
#else
#define WCP_TRACE(...)
#endif

//#define WCP_TRACE_INOUT_PACK


#define	WCB_SHUT_RD SHUT_RD
#define	WCB_SHUT_WR SHUT_WR
#define	WCB_SHUT_RDWR SHUT_RDWR


#define WCP_MTU 1400
#define	WCP_HeaderLen 16
#define	WCP_MDU (WCP_MTU-WCP_HeaderLen)

#define WCP_RCV_BUFFER_MAX (1024*1024)
#define WCP_RCV_BUFFER_MIN (WCP_MDU*4)
#define WCP_RCV_WND_DEFAULT (64*1024)

#define WCP_SND_BUFFER_MAX (1024*1024)
#define WCP_SND_BUFFER_MIN (WCP_MDU*4)
#define WCP_SND_WND_DEFAULT (64*1024)

#define WCP_SND_SSTHRESH_MAX (1024*1024*2)
#define WCP_SND_SSTHRESH_MIN (4*WCP_MTU)
#define WCP_SND_SSTHRESH_DEFAULT WCP_SND_SSTHRESH_MAX


#define WCP_SND_CWND_MAX (WCP_SND_SSTHRESH_MAX*4)

#define WCP_SND_SACK_DUP_COUNT 1

// one seconds
#define WCP_RWND_CHECK_GRANULARITY 1000

#define WCP_SRTT_ALPHA (1/8)
#define WCP_RTTVAR_BETA (1/4)

#define WCP_RTO_CLOCK_GRANULARITY 5
#define WCP_RTO_INIT 1000
#define WCP_RTO_MIN 200
#define WCP_RTO_MAX (60*1000)

#define WCP_LOST_THRESHOLD 3
#define WCP_FAST_RECOVERY_THRESHOLD 2
#define WCP_FAST_RETRANSMIT_GRAULARITY 5


#define WCP_SYN_MAX_TIME 30*1000
#define WCP_FIN_MAX_TIME (2*60*1000)
#define WCP_TIMEWAIT_MAX_TIME (2*60*1000)
#define WCP_LAST_ACK_MAX_TIME (30*1000)

#define WCPPACK_TEST_FLAG( pack, _flag ) (((pack).header.flag)&_flag)

namespace wawo { namespace net {

	using namespace wawo::thread;

	enum WCP_Flag {
		WCP_FLAG_SYN =	1 << 0,
		WCP_FLAG_ACK =	1 << 1,
		WCP_FLAG_SACK =	1 << 2,
		WCP_FLAG_FIN =	1 << 3,
		WCP_FLAG_RST =	1 << 4,
		WCP_FLAG_WND =	1 << 5, //force update wnd any way
		WCP_FLAG_DAT =	1 << 6,
		WCP_FLAG_KEEP_ALIVE =	1 << 7,
		WCP_FLAG_KEEP_ALIVE_REPLY = 1<<8,
	};

	enum WCP_CWND_Cfg {
		WCP_IW = 10,
		WCP_LW = 5,
		WCP_RW = 5
	};

	struct WCP_Header {
		u32_t seq;
		u32_t ack;
		u32_t wnd;
		u16_t flag;
		u16_t dlen;
	};

	struct WCB_pack {
		WCP_Header header;
		WWSP<wawo::packet> data;
		u64_t sent_ts;
		u32_t sent_times;
	};

	struct WCB_received_pack {
		WCP_Header header;
		WWSP<wawo::packet> data;
		address	from;
	};

	typedef std::list <WWSP<WCB_pack>> WCB_PackList;
	typedef std::queue<WWSP<WCB_pack>> WCB_PackQueue;

	typedef std::vector<WWSP<WCB_received_pack>> WCB_ReceivedPackVector;
	typedef std::list<WWSP<WCB_received_pack>> WCB_ReceivedPackList;

	enum WCB_State {
		WCB_CLOSED,
		WCB_LISTEN,
		WCB_SYNING, //this is a middle state lies between WCB_CLOSED and WCB_SYN_SENT, cuz we'll inject pack to net on another thread
		WCB_SYN_SENT,
		WCB_SYN_RECEIVED,
		WCB_FIN_WAIT_1,
		WCB_CLOSING,
		WCB_FIN_WAIT_2,
		WCB_ESTABLISHED,
		WCB_CLOSE_WAIT,
		WCB_TIME_WAIT,
		WCB_LAST_ACK,
		WCB_RECYCLE
	};

	enum WCB_flag {
		RCV_ARRIVE_NEW = 1<<0,

		SND_BIGGEST_SACK_UPDATE = 1<<1,
		SND_UNA_UPDATE = 1<<2,
		SND_FAST_RECOVERED = 1<<3,
		SND_CWND_SLOW_START = 1<<4,
		SND_CWND_CONGEST_AVOIDANCE = 1<<5,
		SND_LOST_DETECTED = 1<<6,

		WCB_FLAG_IS_LISTENER = 1<<7,
		WCB_FLAG_IS_PASSIVE_OPEN = 1<<8,
		WCB_FLAG_IS_ACTIVE_OPEN	= 1<<9,
		WCB_FLAG_FIRST_RTT_DONE = 1<<10,
		WCB_FLAG_CLOSED_CALLED = 1<<11,
	};

	struct WCB_SndInfo {
		u32_t dsn;
		u32_t next;
		u32_t una;

		u32_t ssthresh;
		u32_t cwnd;
		u32_t rwnd;
	};

	struct WCB_RcvInfo {
		u32_t wnd;
		u32_t next;
	};

	enum WCB_Option {
		WCP_O_NONBLOCK = 1 << 0,
	};

	enum READ_flag {
		READ_RWND_LESS_THAN_MTU			= 1,
		READ_RWND_MORE_THAN_MTU			= 1<<1,
		READ_REMOTE_FIN_RECEIVED		= 1<<2,
		READ_REMOTE_FIN_READ_BY_USER	= 1<<3,
		READ_LOCAL_READ_SHUTDOWNED		= 1<<4,
		READ_RECV_ERROR					= 1<<5
	};

	enum WRITE_flag {
		WRITE_LOCAL_FIN_SENT			= 1,
		WRITE_LOCAL_WRITE_SHUTDOWNED	= 1<<1,
		WRITE_SEND_ERROR				= 1<<2
	};


	struct WCB;
	typedef std::vector< WWRP<WCB> > WCBList;
	typedef std::queue< WWRP<WCB> > WCBQueue;

	class socket;

	struct WCB_keepalive_vals {
		u16_t idle;
		u16_t interval;
		u16_t probes;
	};

	//wcb block
	struct WCB:
		public wawo::ref_base
	{
		WWRP<socket> so;
		int fd;
		wawo::u32_t four_tuple_hash_id;
		spin_mutex mutex;
		condition_any cond;
		WCB_State state;
		u64_t timer_state;

		int wcb_errno;
		int wcb_option;

		u16_t wcb_flag;
		u8_t r_flag;
		u8_t s_flag;

		wawo::net::address local_addr;
		wawo::net::address remote_addr;

		u32_t rto;
		u32_t srtt;
		u32_t rttvar;

		spin_mutex received_vec_mutex;
		WWSP<WCB_ReceivedPackVector> received_vec;
		WWSP<WCB_ReceivedPackVector> received_vec_standby;

		WCB_RcvInfo rcv_info;
		WCB_ReceivedPackList rcv_received;

		spin_mutex r_mutex;
		condition_any r_cond;
		WWRP<wawo::bytes_ringbuffer> rb;
		WWRP<wawo::bytes_ringbuffer> rb_standby;
		u64_t r_timer_last_rwnd_update;

		WCBList backloglist_pending;
		WCBQueue backlogq;
		u32_t backlog_size;

		u32_t snd_biggest_ack;
		u64_t snd_lost_detected_timer;
		u32_t snd_lost_biggest_seq;
		u32_t snd_lost_cwnd_for_fast_recovery_compensation;
		u8_t  snd_lost_bigger_than_lost_seq_time;

		u64_t snd_timer_cwnd_congest_avoidance;
		WCB_SndInfo snd_info;
		WCB_PackQueue snd_sending;

		u32_t snd_nflight_bytes;
		u32_t snd_nflight_bytes_max;
		WCB_PackList snd_flights;
		u64_t snd_last_rto_timer;
		WWSP<wawo::packet> snd_sacked_pack_tmp;

		spin_mutex s_mutex;
		WWRP<wawo::bytes_ringbuffer> sb;
		WCB_PackQueue s_sending_standby;

		WCB_PackQueue s_sending_ignore_seq_space;

		u64_t keepalive_timer_last_received_pack;
		WCB_keepalive_vals keepalive_vals;
		u8_t keepalive_probes_sent;

		WCB();
		~WCB();

		void init() {
			lock_guard<spin_mutex> lg(mutex);

			timer_state = 0;
			state = WCB_CLOSED;

			wcb_flag = 0;
			wcb_option = 0;
			wcb_errno = 0;

			rto = WCP_RTO_INIT;

			received_vec = wawo::make_shared<WCB_ReceivedPackVector>();
			received_vec_standby = wawo::make_shared<WCB_ReceivedPackVector>();

			received_vec->reserve(256);
			received_vec_standby->reserve(256);

			rcv_info.next = 0;
			rcv_info.wnd = WCP_RCV_WND_DEFAULT;

			wcb_flag = 0;
			r_flag = 0;
			s_flag = 0;

			r_timer_last_rwnd_update = 0;

			snd_biggest_ack = 0;
			wcb_flag = SND_CWND_SLOW_START;
			snd_lost_detected_timer = 0;
			snd_lost_biggest_seq = 0;

			snd_info.dsn = 0;
			snd_info.next = 0;
			snd_info.una = 0;
			snd_info.ssthresh = WCP_SND_SSTHRESH_DEFAULT;
			snd_info.cwnd = WCP_CWND_Cfg::WCP_IW*WCP_MTU;
			snd_info.rwnd = WCP_RCV_WND_DEFAULT;

			snd_timer_cwnd_congest_avoidance = 0;

			snd_nflight_bytes = 0;
			snd_nflight_bytes_max = 0;
			snd_last_rto_timer = 0;

			snd_sacked_pack_tmp = wawo::make_shared<wawo::packet>(64*1024);

			WAWO_ASSERT(rb == NULL);
			WAWO_ASSERT(rb_standby == NULL);
			WAWO_ASSERT(sb == NULL);

			rb = wawo::make_ref<wawo::bytes_ringbuffer>(WCP_RCV_WND_DEFAULT);
			rb_standby = wawo::make_ref<wawo::bytes_ringbuffer>(WCP_RCV_WND_DEFAULT);

			sb = wawo::make_ref<wawo::bytes_ringbuffer>(WCP_SND_WND_DEFAULT);

			backlog_size = 128;

			keepalive_timer_last_received_pack = wawo::time::curr_milliseconds();
			keepalive_vals.idle = 60 ;
			keepalive_vals.interval = 60;
			keepalive_vals.probes = 5;
			keepalive_probes_sent = 0;
		}

		void deinit() {
			WAWO_ASSERT(rb == NULL);
			WAWO_ASSERT(sb == NULL);

			rb = NULL;
			rb_standby = NULL;
			sb = NULL;
		}

		int get_rcv_buffer_size() const {
			return (rb->capacity()>>1);
		}

		int set_rcv_buffer_size(int const& size) {
			lock_guard<spin_mutex> lg( mutex ) ;

			if (state != WCB_CLOSED) {
				return wawo::E_INVALID_OPERATION;
			}

			u32_t _size = WAWO_MIN( size, WCP_RCV_BUFFER_MAX );
			_size = WAWO_MAX(_size, WCP_RCV_BUFFER_MIN);

			WAWO_ASSERT( rb->count() == 0 );
			rb = wawo::make_ref<bytes_ringbuffer>(_size<<1);
			WAWO_ALLOC_CHECK(rb, sizeof(bytes_ringbuffer));

			WAWO_ASSERT(rb_standby->count() == 0);
			rb_standby = wawo::make_ref<bytes_ringbuffer>(_size<<1);

			rcv_info.wnd = rb->capacity();

			return wawo::OK;
		}

		int get_snd_buffer_size() const {
			return (sb->capacity() >> 1);
		}

		int set_snd_buffer_size(int const& size) {
			lock_guard<spin_mutex> lg(mutex);

			if (state != WCB_CLOSED) {
				return wawo::E_INVALID_OPERATION;
			}

			u32_t _size = WAWO_MIN(size, WCP_SND_BUFFER_MAX);
			_size = WAWO_MAX(_size, WCP_SND_BUFFER_MIN);

			WAWO_ASSERT(sb->count() == 0);
			sb = wawo::make_ref<bytes_ringbuffer>(_size<<1);
			WAWO_ALLOC_CHECK(sb,sizeof(bytes_ringbuffer));

			return wawo::OK;
		}

		inline void SYN() {
			WAWO_ASSERT(state == WCB_CLOSED);
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn++;
			opack->header.flag = WCP_FLAG_SYN;
			opack->header.dlen = 0;
			s_sending_standby.push(opack);
		}

		inline void SYNACK() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn++;
			opack->header.flag = WCP_FLAG_ACK;
			opack->header.dlen = 0;
			s_sending_standby.push(opack);
		}

		inline void SYNSYNACK() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn++;
			opack->header.flag = WCP_FLAG_SYN | WCP_FLAG_ACK;
			opack->header.dlen = 0;

			s_sending_standby.push(opack);
		}

		inline void FIN() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn++;
			opack->header.flag = WCP_FLAG_FIN ;
			opack->header.dlen = 0;
			s_sending_standby.push(opack);
		}

		inline void FINACK() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn++;
			opack->header.flag = WCP_FLAG_ACK;
			opack->header.dlen = 0;
			s_sending_standby.push(opack);
		}

		inline void SACK(WWSP<wawo::packet> const& sacked_packet) {
			static u32_t pack_acked_max_bytes = (WCP_MDU / sizeof(u32_t) )*sizeof(u32_t);

			while ( sacked_packet->len() ) {
				WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
				WWSP<packet> data = wawo::make_shared<wawo::packet>(WCP_MDU);
				u32_t wlen = sacked_packet->len()>=pack_acked_max_bytes ? pack_acked_max_bytes : sacked_packet->len() ;
				data->write( sacked_packet->begin(), wlen );
				sacked_packet->skip(wlen);

				opack->header.seq = snd_info.dsn;
				opack->header.flag = WCP_FLAG_SACK;
				opack->header.dlen = data->len() & 0xFFFF;
				opack->data = data;
				s_sending_ignore_seq_space.push(opack);

				for (int i = 0; i < WCP_SND_SACK_DUP_COUNT; ++i) {
					s_sending_ignore_seq_space.push(opack);
				}
			}
		}

		inline void keepalive() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn;
			opack->header.flag = WCP_FLAG_KEEP_ALIVE;
			opack->header.dlen = 0;
			s_sending_ignore_seq_space.push(opack);
		}

		inline void keepalive_reply() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn;
			opack->header.flag = WCP_FLAG_KEEP_ALIVE_REPLY;
			opack->header.dlen = 0;
			s_sending_ignore_seq_space.push(opack);
		}

		inline void update_rwnd() {
			WWSP<WCB_pack> opack = wawo::make_shared<WCB_pack>();
			opack->header.seq = snd_info.dsn;
			opack->header.flag = WCP_FLAG_WND ;
			opack->header.dlen = 0;
			s_sending_ignore_seq_space.push(opack);
		}

		inline void SYN_RCVD(WWSP<WCB_received_pack> const& synpack) {
			lock_guard<spin_mutex> _lg(mutex);
			WAWO_ASSERT(state == WCB_CLOSED);

			lock_guard<spin_mutex> lg_received_vec(received_vec_mutex);
			received_vec_standby->push_back(synpack);
			state = WCB_SYN_RECEIVED;
		}

		inline void _check_cwnd(u32_t const& check_size ) {
			if (wcb_flag&SND_CWND_SLOW_START) {
				snd_info.cwnd += check_size;

				if (snd_info.cwnd >= snd_info.ssthresh) {
					wcb_flag |= SND_CWND_CONGEST_AVOIDANCE;
					wcb_flag &= ~SND_CWND_SLOW_START;
				}
			}
			else if (wcb_flag&SND_CWND_CONGEST_AVOIDANCE) {
				u64_t now = wawo::time::curr_milliseconds();
				if ((now - snd_timer_cwnd_congest_avoidance) >= static_cast<u32_t>(srtt)) {
					snd_info.cwnd += WCP_MTU;
					snd_timer_cwnd_congest_avoidance = now;
				}
			}
			else {}

			snd_info.cwnd = WAWO_MIN(snd_info.cwnd, WCP_SND_CWND_MAX);
		}

		inline void _handle_pack_acked(WCB_PackList::iterator const& otf_it, u64_t const& now) {
			//rtt sample
			if ((*otf_it)->sent_times == 1) {
				u32_t rtt = static_cast<i32_t>(now-(*otf_it)->sent_ts);
				WAWO_ASSERT(rtt >= 0);

				if ( !(wcb_flag&WCB_FLAG_FIRST_RTT_DONE) ) {
					wcb_flag |= WCB_FLAG_FIRST_RTT_DONE;
					srtt = rtt;
					rttvar = rtt >> 1;
					rto = srtt + WAWO_MAX(WCP_RTO_CLOCK_GRANULARITY, 2*rttvar);
					WCP_TRACE("[wcp][rto]first rto calc: rto: %u, srtt: %u, rttvar: %u", rto, srtt, rttvar);
				}
				else {
					rttvar = static_cast<u32_t>((1-WCP_RTTVAR_BETA)*rttvar + WCP_RTTVAR_BETA*WAWO_ABS2(srtt,rtt));
					srtt = static_cast<u32_t>((1-WCP_SRTT_ALPHA)*srtt + WCP_SRTT_ALPHA*rtt);
					rto = static_cast<u32_t>(srtt + WAWO_MAX(WCP_RTO_CLOCK_GRANULARITY, 2*rttvar));
				}

				if (rto > WCP_RTO_MAX) {
					rto = WCP_RTO_MAX;
				}
				else if (rto<WCP_RTO_MIN) {
					rto = WCP_RTO_MIN;
				}
				else {}
			}

			//cwnd check
			u32_t ntotal_bytes = (*otf_it)->header.dlen + WCP_HeaderLen;
			WAWO_ASSERT( snd_nflight_bytes >= ntotal_bytes);
			snd_nflight_bytes -= ntotal_bytes;
			_check_cwnd(ntotal_bytes);
		}

		WCB_State update(u64_t const& now);
		void pump_packs();

		void check_recv(u64_t const& now);
		void check_flights( u64_t const& now);
		void check_send(u64_t const& now);


		int send_pack(WWSP<WCB_pack> const& pack);

		void listen_handle_syn();
		int recv_pack(WWSP<WCB_received_pack>& pack, address& from);

		int connect(wawo::net::address const& addr);
		int shutdown(int const& flag);
		int close();
		int listen( int const& backlog );
	};

	enum wpoll_op {
		WPOLL_CTL_ADD, //add flag
		WPOLL_CTL_DEL, //del flag
	};

	enum wpoll_events {
		WPOLLIN		=0x01,
		WPOLLOUT	=0x02,
		WPOLLERR	=0x04,
		WPOLLHUB	=0x08 //unexpected close
	};

	struct wpoll_watch_event {
		int fd;
		u32_t evts;
		WWRP<ref_base> cookie;
		WWRP<WCB> wcb;
	};

	struct wpoll_event {
		int fd;
		u32_t evts;
		WWRP<ref_base> cookie;
	};

	struct wpoll_event_pending {
		wpoll_event wpoll_evt;
		WWSP<wpoll_watch_event> watch_evt;
	};

	typedef std::vector< WWSP<wpoll_watch_event> > wpoll_watch_event_vector;
	typedef std::queue< wpoll_event_pending > wpoll_event_pending_queue;
	struct wpoll {
		shared_mutex mutex;
		wpoll_watch_event_vector evts;
	};

	typedef std::map<int, WWSP<wpoll>> WpollMap;
	typedef std::pair<int, WWSP<wpoll>> WpollPair;

	class wcp :
		public wawo::singleton<wcp>
	{
		enum State {
			S_IDLE,
			S_RUN,
			S_EXIT
		};

		enum OpCode {
			OP_NONE,
			OP_WATCH,
		};

		struct Op {
			WWRP<WCB> wcb;
			OpCode op;
		};

		typedef std::queue<Op> OpQueue;
		typedef std::map<int, WWRP<WCB>> WCBMap;
		typedef std::pair<int, WWRP<WCB>> WCBPair;

		static std::atomic<int> s_wcb_auto_increament_id;

		shared_mutex m_mutex;
		WpollMap m_wpoll_map;
		wpoll_event_pending_queue m_event_pending_queue;

		u8_t m_state;

		shared_mutex m_wcb_map_mutex;
		WCBMap m_wcb_map;

		spin_mutex m_wcb_create_pending_map_mutex;
		WCBMap m_wcb_create_pending_map;

		typedef std::unordered_map<wawo::u32_t, WWRP<WCB>> FourTupleWCBMap;
		typedef std::pair<wawo::u32_t, WWRP<WCB>> FourTupleWCBPair;

		spin_mutex m_wcb_four_tuple_map_mutex;
		FourTupleWCBMap m_wcb_four_tuple_map;

		spin_mutex m_ops_mutex;
		OpQueue m_ops;

		WWSP<wawo::thread::fn_ticker> m_self_ticker;

		static std::atomic<int> s_wpoll_auto_increament_id;
	public:
		wcp();
		~wcp();

		int start() { on_start(); return wawo::OK; }

		void on_start();

		void stop() {
			{
				lock_guard<shared_mutex> lg(m_mutex);
				if (m_state == S_EXIT) { return; }
			}
			on_stop();
		}
		void on_stop();

		void run() {
			shared_lock_guard<shared_mutex> lg(m_mutex);
			if (m_state != S_RUN) { return; }
			_execute_ops();
			_update();
		}

		void watch(WWRP<WCB> const& wcb) {
			Op op = { wcb, OP_WATCH };
			_plan_op(op);
		}

		void _update();

	private:
		inline void _plan_op(Op const& op) {
			lock_guard<spin_mutex> lg(m_ops_mutex);
			m_ops.push(op);
		}

		inline void _pop_op(Op& op) {
			lock_guard<spin_mutex> lg_ops(m_ops_mutex);
			if (m_ops.empty()) return;
			op = m_ops.front();
			m_ops.pop();
		}

		void _execute_ops();

	public:

#define WCP_FD_MAX (2048)

		static inline int make_wcb_fd() {
			return (wawo::atomic_increment(&s_wcb_auto_increament_id) % WCP_FD_MAX) + 0xFFFFFF;
		}

		int socket(int const& family, int const& socket_type, int const& protocol);
		int connect(int const& fd, const struct sockaddr* addr, socklen_t const& length);
		int bind(int const& fd, const struct sockaddr* addr, socklen_t const& length);
		int listen(int const& fd, int const& backlog);
		int accept(int const& fd, struct sockaddr* addr, socklen_t* addrlen);
		int shutdown(int const& fd, int const& flag);
		int close(int const& fd);

		int send(int const& fd, byte_t const* const buffer, u32_t const& len, int const& flag);
		int recv(int const& fd, byte_t* const buffer_o, u32_t const& size, int const& flag);


		int getsockname(int const& fd, struct sockaddr* addr, socklen_t* addrlen);

		int getsockopt(int const& fd, int const& level, int const& option_name, void* value, socklen_t* option_length);
		int setsockopt(int const& fd, int const& level, int const& option_name, void const* value, socklen_t const& option_length);

		int fcntl_getfl(int const& fd, int& flag);
		int fcntl_setfl(int const& fd, int const& flag);

		int wpoll_create();
		int wpoll_close(int const& wpoll_handle);
		int wpoll_ctl(int const& wpoll_handle, wpoll_op const& op, wpoll_event const& evt );
		int wpoll_wait(int const& wpoll_handle, wpoll_event evts[], u32_t const& size);


		int add_to_four_tuple_hash_map(WWRP<WCB> const& wcb) {
			lock_guard<spin_mutex> lg_four_tuple_map(m_wcb_four_tuple_map_mutex);
			WAWO_ASSERT( wcb != NULL) ;
			WAWO_ASSERT( wcb->four_tuple_hash_id != 0 );

			if (m_wcb_four_tuple_map.size() >= WCP_FD_MAX) {
				wawo::set_last_errno(wawo::E_EMFILE);
				return wawo::E_EMFILE;
			}

			FourTupleWCBMap::iterator const& it = m_wcb_four_tuple_map.find(wcb->four_tuple_hash_id);
			if (it != m_wcb_four_tuple_map.end()) {
				wawo::set_last_errno(wawo::E_EADDRINUSE);
				return wawo::E_EADDRINUSE;
			}
			m_wcb_four_tuple_map.insert({ wcb->four_tuple_hash_id,wcb });
			return wawo::OK;
		}
		int remove_from_four_tuple_hash_map(WWRP<WCB> const& wcb) {
			lock_guard<spin_mutex> lg_four_tuple_map(m_wcb_four_tuple_map_mutex);
			WAWO_ASSERT(wcb != NULL);
			WAWO_ASSERT(wcb->four_tuple_hash_id != 0);
			WAWO_ASSERT(wcb->wcb_flag&WCB_FLAG_IS_PASSIVE_OPEN);

			FourTupleWCBMap::iterator const& it = m_wcb_four_tuple_map.find(wcb->four_tuple_hash_id);
			if (it == m_wcb_four_tuple_map.end()) {
				return -1;
			}
			m_wcb_four_tuple_map.erase(it);
			return wawo::OK;
		}
	};
}}
#endif
