#ifndef _WAWO_NET_SOCKET_HPP_
#define _WAWO_NET_SOCKET_HPP_

#include <queue>

#include <wawo/smart_ptr.hpp>
#include <wawo/time/time.hpp>
#include <wawo/thread/mutex.hpp>
#include <wawo/net/dispatcher_abstract.hpp>

#include <wawo/packet.hpp>
#include <wawo/bytes_ringbuffer.hpp>
#include <wawo/net/address.hpp>

#include <wawo/net/tlp_abstract.hpp>
#include <wawo/net/net_event.hpp>

#include <wawo/net/socket_base.hpp>

#define WAWO_MAX_ASYNC_WRITE_PERIOD	(90000L) //90 seconds

namespace wawo { namespace net {

	enum SocketFlag {
		F_RCV_ALWAYS_PUMP				= 0x1000000,
		F_RCV_BLOCK_UNTIL_PACKET_ARRIVE = 0x2000000,
		F_SEND_USE_SND_BUFFER			= 0x4000000
	};

	struct socket_event;
	class socket:
		public socket_base,
		public dispatcher_abstract<socket_event>
	{
		WWRP<wawo::bytes_ringbuffer> m_sb; // buffer for send
		WWRP<wawo::bytes_ringbuffer> m_rb; // buffer for recv

		byte_t* m_tsb; //tmp send buffer
		byte_t* m_trb; //tmp read buffer

		u64_t m_delay_wp;
		u64_t m_async_wt;

		spin_mutex m_rps_q_mutex;
		spin_mutex m_rps_q_standby_mutex;

		std::queue<WWSP<wawo::packet>> *m_rps_q;
		std::queue<WWSP<wawo::packet>> *m_rps_q_standby;

		void _init();
		void _deinit();
		typedef dispatcher_abstract<socket_event> _dispatcher_t;

	public:
		explicit socket(int const& fd, address const& addr, socket_mode const& sm, socket_buffer_cfg const& sbc ,family const& family , sock_type const& type, protocol_type const& proto, Option const& option = OPTION_NONE ):
			socket_base(fd,addr,sm,sbc,family,type,proto,option),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL)
		{
			_init();
		}

		explicit socket(family const& family, sock_type const& type, protocol_type const& proto, Option const& option = OPTION_NONE) :
			socket_base(family, type, proto, option),
			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL)
		{
			_init();
		}

		explicit socket(socket_buffer_cfg const& sbc, family const& family, sock_type const& type, protocol_type const& proto, Option const& option = OPTION_NONE) :
			socket_base(sbc, family, type, proto, option),

			m_sb(NULL),
			m_rb(NULL),

			m_tsb(NULL),
			m_trb(NULL),
			m_delay_wp(WAWO_MAX_ASYNC_WRITE_PERIOD),
			m_async_wt(0),
			m_rps_q(NULL),
			m_rps_q_standby(NULL)
		{
			_init();
		}

		~socket() 
		{
			_deinit();
		}

		inline bool is_flush_timer_expired( u64_t const& now /*in milliseconds*/ ) {
			if (m_async_wt == 0) return false;
			lock_guard<spin_mutex> lg(m_mutexes[L_WRITE]);
			return ((m_sb->count()>0) && (m_async_wt != 0) && (now>(m_async_wt+m_delay_wp)));
		}

		int close(int const& ec=0);
		int shutdown(u8_t const& flag, int const& ec=0);

		u32_t accept( WWRP<socket> sockets[], u32_t const& size, int& ec_o ) ;

		u32_t send( byte_t const* const buffer, u32_t const& size, int& ec_o, int const& flag = 0) ;
		u32_t recv( byte_t* const buffer_o, u32_t const& size, int& ec_o, int const& flag = 0 ) ;

		void flush(u32_t& left, int& ec_o, int const& block_time = __FLUSH_DEFAULT_BLOCK_TIME__ /* in microseconds , -1 == INFINITE */ ) ;

		void handle_async_handshake(int& ec_o);
		void handle_async_read(int& ec_o);
		void handle_async_write( int& ec_o);

		//@todo, watch and read/send one time
		void async_read(WWRP<ref_base> const& cookie, wawo::net::fn_io_event const& fn, wawo::net::fn_io_event_error const& err);
		void async_send(WWRP<ref_base> const& cookie, wawo::net::fn_io_event const& fn, wawo::net::fn_io_event_error const& err);

		int send_packet(WWSP<wawo::packet> const& packet, int const& flags = F_SEND_USE_SND_BUFFER );
		u32_t receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o, int const& flag= F_RCV_BLOCK_UNTIL_PACKET_ARRIVE );

		int tlp_handshake(WWRP<ref_base> const& cookie = NULL, fn_io_event const& success = NULL, fn_io_event_error const& error = NULL);

	private:
		u32_t _receive_packets(WWSP<wawo::packet> arrives[], u32_t const& size, int& ec_o, int const& flag );
		void _pump(int& ec_o,int const& flag = 0);
		u32_t _flush(u32_t& left, int& ec_o);

		inline void __rdwr_check(int const& ec = 0) {
			u8_t nflag = 0;
			{
				lock_guard<spin_mutex> lg_r(m_mutexes[L_READ]);
				if (m_rflag& SHUTDOWN_RD) {
					nflag |= SHUTDOWN_RD;
				}
			}
			{
				lock_guard<spin_mutex> lg_w(m_mutexes[L_WRITE]);
				if (m_wflag& SHUTDOWN_WR) {
					nflag |= SHUTDOWN_WR;
				}
			}
			if (nflag == SHUTDOWN_RDWR) {
				close(ec);
			}
		}
	};
}}
#endif