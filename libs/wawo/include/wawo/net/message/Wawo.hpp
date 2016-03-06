#ifndef _WAWO_NET_MESSAGE_WAWO_HPP_
#define _WAWO_NET_MESSAGE_WAWO_HPP_

//save one bytes if on
#define WAWO_COMPACT_ID_AND_TYPE

#define WAWO_MESSAGE_SECURE_MODE 1
#define WAWO_MESSAGE_TIMEOUT_TIME (30*1000)

#include <wawo/core.h>
#include <wawo/net/core/Event.hpp>

namespace wawo { namespace net { namespace message {

	using namespace wawo::net;
	using namespace wawo::net::core;

	// about 49710/s , can run one day for no-repeat
	static std::atomic<wawo::uint32_t> s_ai_nid(1) ; //auto increment
	static wawo::uint32_t MakeNetId() { return s_ai_nid++ ; }

	class Wawo {

	public:

		enum MessageEventId {
			ME_SENT = 0,
			ME_RESPONSE,
			ME_CANCEL,
			ME_TIMEOUT,
			ME_ERROR,
			ME_MAX
		};

		struct Callback_Abstract :
			public wawo::RefObject_Abstract
		{
			virtual bool operator() ( MessageEventId const& id, WAWO_SHARED_PTR<Packet> const& packet ) = 0;
		};

		/*
		//return true to stop propagation
		typedef std::function<bool (WAWO_REF_PTR<Event> const& evt)> WAWO_MESSAGE_CALLBACK;

		struct Callbacks {
			WAWO_REF_PTR<Callbacks_Abstract> callbacks[ME_MAX];
			Callbacks() {
				for( int i=0;i<ME_MAX;i++ ) {
					callbacks[i] = NULL;
				}
			}
			inline WAWO_REF_PTR<Callbacks_Abstract>& operator[] ( int const& evt_id ) {
				WAWO_ASSERT( evt_id < ME_MAX && evt_id >= 0 );
				return callbacks[evt_id];
			}
			inline bool operator() ( WAWO_REF_PTR<Event> const& evt ) {
				int const& id = evt->GetId();
				WAWO_ASSERT( id < ME_MAX );
				if( callbacks[id] != NULL ) {
					return (*callbacks[id])( evt ) ;
				}
				return false;
			}
		};
		*/

	public:
		enum Type {
			T_NONE = 0,
			T_RESPONSE, // response for request
			T_REQUEST, //send & expect response
			T_SEND, //send & dont expect response
			T_MAX
		};

	private:
		Type m_type; //response, request, send
		uint32_t m_net_id; //unique message id

		uint32_t m_id;
		WAWO_SHARED_PTR<Packet> m_packet ;

		uint32_t m_timeout_time;
		uint64_t m_creation_time;
		uint64_t m_arrive_time;
		uint64_t m_send_time;
	public:


		inline void SetTimeoutTime( int const& time /* in milliseconds, -1 == infinite*/ ) { m_timeout_time = time ;}
		inline uint32_t const& GetNetId() {return m_net_id;}
		inline void SetNetId(uint32_t const& id) { m_net_id = id;}
		inline Type const& GetType() {return m_type;}
		inline void SetType( Type const& type ) { m_type = type;}

		//for create
		explicit Wawo( uint32_t const& id ):
			m_type(T_NONE),
			m_net_id(MakeNetId()),
			m_id(id),
			m_packet(NULL),
			m_timeout_time(WAWO_MESSAGE_TIMEOUT_TIME),
			m_creation_time(wawo::time::curr_milliseconds()),
			m_arrive_time(0),
			m_send_time(0)
		{
		}

		//for create
		explicit Wawo( uint32_t const& id, WAWO_SHARED_PTR<Packet> const& packet ):
			m_type( T_NONE ),
			m_net_id( MakeNetId() ),
			m_id(id),
			m_packet(packet),
			m_timeout_time(WAWO_MESSAGE_TIMEOUT_TIME),
			m_creation_time(wawo::time::curr_milliseconds()),
			m_arrive_time(0),
			m_send_time(0)
		{
		}

		explicit Wawo(uint32_t const& net_id, uint32_t const& id,Type const& type, WAWO_SHARED_PTR<Packet> const& packet ):
			m_type( type ),
			m_net_id( net_id ),
			m_id(id),
			m_packet(packet),
			m_timeout_time(WAWO_MESSAGE_TIMEOUT_TIME),
			m_creation_time(0),
			m_arrive_time(0),
			m_send_time(0)
		{
			WAWO_ASSERT( type != T_NONE);
			m_arrive_time = m_creation_time = wawo::time::curr_milliseconds();
		}

		inline uint32_t const& GetId() const {
			return m_id;
		}

		inline WAWO_SHARED_PTR<Packet> const& GetPacket() const {
			return m_packet;
		}

		//each type of connection must implement the following two func
		static int Encode( WAWO_SHARED_PTR<Wawo> const& message_in, WAWO_SHARED_PTR<Packet>& packet_out ) {
			// format
			// message_len(len of [net_id+id+type+message], net-id, id, type, message
			//WAWO_CONDITION_CHECK( message_in->GetNetId() != 0 );
			WAWO_CONDITION_CHECK( message_in->GetId() >= 0 );
			WAWO_ASSERT( message_in->GetId() < 64 ); //2^6 == 64 (0,63]

			WAWO_CONDITION_CHECK( message_in->GetType() != T_NONE );
			WAWO_CONDITION_CHECK( message_in->GetType() < T_MAX );

			WAWO_ASSERT( message_in->GetPacket() != NULL );
			WAWO_ASSERT( message_in->GetPacket()->Length() > 0 );

#ifdef WAWO_COMPACT_ID_AND_TYPE
			uint8_t id_type = message_in->GetType()&0x03; // ---- --11
			id_type |= (message_in->GetId()&0xFF)<<2;
#endif

#ifdef WAWO_MESSAGE_SECURE_MODE
			WAWO_SHARED_PTR<Packet> packet( new Packet( message_in->GetPacket()->Length()) );
#else
			WAWO_REF_PTR<Packet>& packet = message_in->GetPacket();
#endif

#ifdef WAWO_COMPACT_ID_AND_TYPE
			packet->WriteLeft<uint8_t>(id_type);
#else
			packet->WriteLeft<uint8_t>(message->GetId()&0xFF);
			packet->WriteLeft<uint8_t>(message->GetType()&0XFF);
#endif
			packet->WriteLeft<uint32_t>(message_in->GetNetId());

			packet->Write( message_in->GetPacket()->Begin(), message_in->GetPacket()->Length() );

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
			uint32_t len = packet->Length() ;
			WAWO_ASSERT( len <= (wawo::limit::UINT16_MAX_V&0xFFFF) );
			if( len > (wawo::limit::UINT16_MAX_V&0xFFFF) ) {
				ec_o = wawo::E_NET_MESSAGE_EXCEED_LENGTH ;
				return WAWO_REF_PTR<Packet>::NULL_PTR;
			}
			packet->WriteLeft<uint16_t>(len&0xFFFF);
#else
			WAWO_ASSERT( packet->Length() <= (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) );
			if( packet->Length() > (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) ) {
				return wawo::E_NET_MESSAGE_EXCEED_LENGTH ;
			}
			packet->WriteLeft<uint32_t>(packet->Length());
#endif
			packet_out = packet;
			return wawo::OK;
		}

		//return received packet count, set ec code if any
		static uint32_t Decode( WAWO_SHARED_PTR<Packet> const& packet_in, WAWO_SHARED_PTR<Wawo> messages_out[], uint32_t const& size, int& ec_o ) {
			//@todo
			//optimize method:
			//1, COW packet implement
			ec_o = wawo::OK;
			uint32_t count = 0 ;
			do {

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
				uint32_t mlen = (packet_in->Read<wawo::uint16_t>()&0xFFFF) ;
#else
				uint32_t mlen = packet_in->Read<wawo::uint32_t>() ;
#endif
				WAWO_CONDITION_CHECK( packet_in->Length() >= mlen ) ;

				if( mlen == packet_in->Length() ) {

					uint32_t net_id = packet_in->Read<uint32_t>();

#ifdef WAWO_COMPACT_ID_AND_TYPE
					uint8_t id_type = packet_in->Read<uint8_t>();
					Type type	= (Type)(id_type&0x03);
					uint8_t id	= ( (id_type>>2)&0xFF );
#else
					uint8_t id = packet_in->Read<uint8_t>();
					Type type = (Type)(packet_in->Read<uint8_t>());
#endif

					//for the case on net_message one packet
					messages_out[count++] = WAWO_SHARED_PTR<Wawo> ( new Wawo(net_id,id,type,packet_in) );
					break;
				} else {
					//one packet , mul-messages
					//copy and assign
					WAWO_ASSERT( "@TODO" );
				}
			} while( packet_in->Length() );

			WAWO_ASSERT ( (count <= size) ? true : packet_in->Length() == 0 );
			return count ;
		}
	};

	typedef Wawo WawoMessage;
}}}
#endif
