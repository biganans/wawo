#ifndef _WAWO_NET_PROTOCOL_ICMP_HPP_
#define _WAWO_NET_PROTOCOL_ICMP_HPP_

#include <wawo/core.hpp>
#include <wawo/app.hpp>

#include <wawo/net/socket.hpp>

namespace wawo { namespace net { namespace protocol { namespace ipv4 {

	struct IPHeader {
		u8_t	VER_IHL;
		u8_t	TOS;
		u16_t	total_length;
		u16_t	id;
		u16_t	FlagFragoffset;
		u8_t	TTL;
		u8_t	protocol;
		u16_t	checksum;
		u32_t	src;
		u32_t	dst;
	};

	enum ICMPType {
		T_ECHO_REPLY = 0,
		T_DESTINATION_UNREACHABLE = 3,
		T_SOURCE_QUENCH = 4,
		T_REDIRECT = 5,
		T_ECHO = 8,
		T_TIME_EXCEEDED = 11,
		T_PARAMETER_PROBLEM = 12,

		T_TIMESTAMP = 13,
		T_TIMESTAMP_REPLY = 14,
		T_INFORMATION_REQUEST = 15,
		T_INFORMATION_REQUEST_REPLY = 16
	};

	enum ICMPCode {
		C_NET_UNREACHABLE = 0,
		C_HOST_UNREACHABLE,
		C_PROTOCOL_UNREACHABLE,
		C_PORT_UNREACHABLE,
		C_FRAGMENTATION_NEEDED_AND_DF_SET,
		C_SOURCE_ROUTE_FAILED
	};


	struct ICMP_Echo {

		wawo::byte_t	type;
		wawo::byte_t	code;
		wawo::u16_t 	checksum;
		wawo::u16_t	id;
		wawo::u16_t	seq;

		wawo::u64_t	ts;	//our data
	};


	struct PingReply {
		wawo::u16_t seq;
		wawo::u32_t RTT;
		wawo::u32_t bytes;
		wawo::u32_t TTL;
	};

	static std::atomic<wawo::u16_t> __pingseq(1);
	static wawo::u16_t ping_make_seq() {
		return wawo::atomic_increment<wawo::u16_t>(&__pingseq);
	}

	class ICMP {

	private:
		WWRP<wawo::net::socket> m_so;
		wawo::byte_t* icmp_data;

	public:
		ICMP() :
			m_so(NULL)
		{
		}
		~ICMP() {}

		int ping( char const * ip, PingReply& reply, u32_t timeout)
		{

			(void)timeout;
			int initrt = _init_socket();
			WAWO_RETURN_V_IF_NOT_MATCH(initrt, initrt == wawo::OK);

			ICMP_Echo echo;

			echo.type = ICMPType::T_ECHO;
			echo.code = 0;
			echo.id = wawo::app::app::get_process_id()&0xffff;
			echo.seq = ping_make_seq();
			echo.ts = wawo::time::curr_milliseconds();
			echo.checksum = 0;

			WWRP<wawo::packet> icmp_pack = wawo::make_ref<wawo::packet>();
			icmp_pack->write<u8_t>(echo.type);
			icmp_pack->write<u8_t>(echo.code);
			icmp_pack->write<u16_t>(0);
			icmp_pack->write<u16_t>(echo.id);
			icmp_pack->write<u16_t>(echo.seq);
			icmp_pack->write<u64_t>(echo.ts);

			echo.checksum = _calculate_checksum(icmp_pack->begin(), icmp_pack->len());

			icmp_pack->reset();
			icmp_pack->write<u8_t>(echo.type);
			icmp_pack->write<u8_t>(echo.code);
			icmp_pack->write<u16_t>(echo.checksum);
			icmp_pack->write<u16_t>(echo.id);
			icmp_pack->write<u16_t>(echo.seq);
			icmp_pack->write<u64_t>(echo.ts);

			wawo::net::address addr( ip, 0 );

			int ec;
			u32_t snd_c = m_so->sendto(icmp_pack->begin(), icmp_pack->len(), addr, ec);
			WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);

			(void)snd_c;

			wawo::net::address recv_addr;
			byte_t recv_buffer[256] = { 0 };

			u32_t recv_c = m_so->recvfrom(recv_buffer, 256, recv_addr, ec);
			wawo::u64_t now = wawo::time::curr_milliseconds();
			WAWO_RETURN_V_IF_NOT_MATCH(ec, ec == wawo::OK);
			WAWO_ASSERT(recv_c > 0);

			u32_t read_idx = 0;
			u8_t ver_IHL = wawo::bytes_helper::read_u8(recv_buffer + read_idx);
			read_idx++;

			u32_t IPHeaderLen = (ver_IHL & 0x0f) * 4;

			u32_t ICMPHeader_idx = IPHeaderLen;

			ICMP_Echo echo_reply;

			echo_reply.type = wawo::bytes_helper::read_u8(recv_buffer+ ICMPHeader_idx);
			ICMPHeader_idx++;
			echo_reply.code = wawo::bytes_helper::read_u8(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx++;

			echo_reply.checksum = wawo::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.id = wawo::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.seq = wawo::bytes_helper::read_u16(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += 2;

			echo_reply.ts = wawo::bytes_helper::read_u64(recv_buffer + ICMPHeader_idx);
			ICMPHeader_idx += sizeof(wawo::u64_t);

			if (echo_reply.id == echo.id	&&
				echo_reply.seq == echo.seq	&&
				echo_reply.type == T_ECHO_REPLY
			)
			{
				reply.seq	= echo.seq;
				reply.RTT	= (wawo::u32_t)(now - echo.ts);
				reply.bytes = sizeof(echo.ts)*sizeof(byte_t);
				reply.TTL = wawo::bytes_helper::read_u8(recv_buffer+8);
			}

			return wawo::OK;
		}

	private:

		int _init_socket()
		{
			if (m_so == NULL) {
				m_so = wawo::make_ref<wawo::net::socket>(wawo::net::F_AF_INET, wawo::net::T_RAW, wawo::net::P_ICMP);
				WAWO_ALLOC_CHECK(m_so, sizeof(wawo::net::socket));
				int openrt = m_so->open();
				return openrt;
			}

			return wawo::OK;
		}

		u16_t _calculate_checksum(byte_t* data, u32_t length) {

			u32_t checksum = 0;
			u32_t	_length = length;

			while (_length > 1) {
				checksum += wawo::bytes_helper::read_u16(data + (length - _length));
				_length -= sizeof(u16_t);
			}

			if (_length) {
				checksum += wawo::bytes_helper::read_u8(data+(length - _length));
			}

			checksum = (checksum >> 16) + (checksum & 0xffff);
			checksum += (checksum >> 16);
			return(u16_t)(~checksum);
		}

	};

}}}}

#endif
