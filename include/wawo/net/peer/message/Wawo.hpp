#ifndef _WAWO_NET_MESSAGE_WAWO_HPP_
#define _WAWO_NET_MESSAGE_WAWO_HPP_

#define WAWO_MESSAGE_SECURE_MODE 1
#define WAWO_MESSAGE_TIMEOUT_TIME (30*1000)

#include <wawo/core.h>

namespace wawo { namespace net { namespace peer { namespace message {

	using namespace wawo::net;
	using namespace wawo::net::core;

	typedef u32_t NetIdT;

	// about 49710/s , can run one day for no-repeat
	static std::atomic<NetIdT> s_ai_nid(1); //auto increment
	inline static NetIdT MakeNetId() { return wawo::atomic_increment(&s_ai_nid) ; }

	class Wawo {

	public:
		typedef wawo::net::peer::message::NetIdT NetIdT;
		typedef u8_t TypeT;
		typedef u32_t PLenT;

		enum Type {
			T_NONE = 0,
			T_RESPONSE, // response for request
			T_REQUEST, //send & expect response
			T_SEND, //send & dont expect response
			T_KEEP_ALIVE,
			T_MAX
		};

	private:
		NetIdT m_net_id; //unique message id
		TypeT m_type; //response, request, send
		WWSP<Packet> m_packet;

	public:
		inline NetIdT const& GetNetId() const {return m_net_id;}
		inline void SetNetId(NetIdT const& id) { m_net_id = id;}

		inline TypeT const& GetType() const {return m_type;}
		inline void SetType( TypeT const& type ) { m_type = type;}

		//for create
		explicit Wawo( WWSP<Packet> const& packet ):
			m_net_id( MakeNetId() ),
			m_type( T_NONE ),
			m_packet(packet)
		{
		}

		explicit Wawo(NetIdT const& net_id,TypeT const& type, WWSP<Packet> const& packet ):
			m_net_id( net_id ),
			m_type( type ),
			m_packet(packet)
		{
			WAWO_ASSERT( type != T_NONE);
		}

		virtual ~Wawo() {}

		Wawo( Wawo const& other ):
			m_net_id(other.m_net_id),
			m_type(other.m_type),
			m_packet(NULL)
		{
			if( other.m_packet != NULL ) {
				m_packet = WWSP<Packet>(new Packet( *other.GetPacket() ));
			}
		}

		WWSP<Packet> GetPacket() const { return m_packet; }
		void SetPacket(WWSP<Packet> const& packet) {m_packet=packet;}

		Wawo& operator= ( Wawo const& other ) {
			Wawo(other).Swap( *this );
			return *this;
		}

		void Swap( Wawo& other ) {
			wawo::swap(m_net_id,other.m_net_id);
			wawo::swap(m_type,other.m_type);
			m_packet.swap( other.m_packet );
		}

		//each type of connection must implement the following two func
		virtual int Encode( WWSP<Packet>& packet_o ) {
			// format
			// message_len(len of [net_id+id+type+message], net-id, id, type, message
			//WAWO_CONDITION_CHECK( message_in->GetNetId() != 0 );

			WAWO_ASSERT( GetType() != T_NONE );
			WAWO_ASSERT( GetType() < T_MAX );

			WAWO_ASSERT( GetPacket() != NULL );
			WAWO_ASSERT( GetPacket()->Length() > 0 );


			WWSP<Packet> packet( new Packet( GetPacket()->Length()) );

			packet->WriteLeft<TypeT>(GetType()&0XFF);
			packet->WriteLeft<NetIdT>(GetNetId());

			packet->Write( GetPacket()->Begin(), GetPacket()->Length() );

#ifdef WAWO_LIMIT_MESSAGE_LEN_TO_SHORT
			u32_t len = packet->Length() ;
			WAWO_ASSERT( len <= (wawo::limit::UINT16_MAX_V&0xFFFF) );
			if( len > (wawo::limit::UINT16_MAX_V&0xFFFF) ) {
				ec_o = wawo::E_NET_MESSAGE_EXCEED_LENGTH ;
				return WWRP<Packet>::NULL_PTR;
			}
			packet->WriteLeft<u16_t>(len&0xFFFF);
#else
			WAWO_ASSERT( packet->Length() <= (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) );
			if( packet->Length() > (wawo::limit::UINT32_MAX_V&0xFFFFFFFF) ) {
				return wawo::E_NET_MESSAGE_EXCEED_LENGTH ;
			}
			packet->WriteLeft<PLenT>(packet->Length());
#endif
			packet_o = packet;
			return wawo::OK;
		}

	};

	typedef Wawo WawoMessage;
}}}}
#endif