#ifndef _WAWO_NET_MESSAGE_WAWO_HPP_
#define _WAWO_NET_MESSAGE_WAWO_HPP_

#define WAWO_MESSAGE_SECURE_MODE 1
#define WAWO_MESSAGE_TIMEOUT_TIME (30*1000)

#include <wawo/core.hpp>
#include <wawo/packet.hpp>

namespace wawo { namespace net { namespace peer { namespace message {

	typedef u32_t net_id_t;

	// about 49710/s , can run one day for no-repeat
	static std::atomic<net_id_t> s_ai_ros_nid(1); //auto increment
	inline static net_id_t make_ros_net_id() { return ::wawo::atomic_increment(&s_ai_ros_nid) ; }

	struct ros {

		typedef wawo::net::peer::message::net_id_t net_id_t;
		typedef u8_t type_t;
		typedef u32_t packet_len_t;

		enum ros_type {
			T_NONE = 0,
			T_RESPONSE, // response for request
			T_REQUEST, //send & expect response
			T_SEND, //send & dont expect response
			T_KEEP_ALIVE,
			T_MAX
		};

		net_id_t net_id; //unique message id
		ros_type type; //response, request, send
		WWSP<packet> data;

		//for create
		explicit ros( WWSP<packet> const& data ):
			net_id( make_ros_net_id() ),
			type( T_NONE ),
			data(data)
		{
		}

		explicit ros(net_id_t const& net_id,ros_type const& type, WWSP<packet> const& pack ):
			net_id( net_id ),
			type( type ),
			data(pack)
		{
			WAWO_ASSERT( type != T_NONE);
		}

		virtual ~ros() {}

		ros( ros const& other ):
			net_id(other.net_id),
			type(other.type),
			data(NULL)
		{
			if( other.data != NULL ) {
				data = wawo::make_shared<packet>( *(other.data) );
			}
		}

		ros& operator= ( ros const& other ) {
			ros(other).swap( *this );
			return *this;
		}

		void swap( ros& other ) {
			std::swap(net_id,other.net_id);
			std::swap(type,other.type);
			data.swap( other.data );
		}

		//each type of connection must implement the following two func
		int encode( WWSP<packet>& packet_o ) {
			// format
			// message_length(length of [net_id+id+type+message], net-id, id, type, message
			//WAWO_CONDITION_CHECK( message_in->net_id != 0 );

			WAWO_ASSERT( type != T_NONE );
			WAWO_ASSERT( type < T_MAX );

			WAWO_ASSERT( data != NULL );
			WAWO_ASSERT( data->len() > 0 );


			WWSP<packet> ooo = wawo::make_shared<packet>( data->len() );

			ooo->write_left<type_t>( type&0XFF);
			ooo->write_left<net_id_t>(net_id);

			ooo->write( data->begin(), data->len() );

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
			u32_t length = ooo->len() ;
			WAWO_ASSERT( length <= (wawo::limit::UINT16_MAX_V&0xFFFF) );
			if( length > (wawo::limit::UINT16_MAX_V&0xFFFF) ) {
				ec_o = wawo::E_PEER_MESSAGE_EXCEED_LENGTH ;
				return WWRP<packet>::NULL_PTR;
			}
			ooo->write_left<u16_t>(length&0xFFFF);
#else
			WAWO_ASSERT(ooo->len() <= (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) );
			if(ooo->len() > (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) ) {
				return wawo::E_PEER_MESSAGE_EXCEED_LENGTH ;
			}
			ooo->write_left<packet_len_t>(ooo->len());
#endif
			packet_o = ooo;
			return wawo::OK;
		}

	};

//	typedef ros ros_message;
}}}}
#endif