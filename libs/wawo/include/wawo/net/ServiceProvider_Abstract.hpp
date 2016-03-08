#ifndef _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_
#define _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/ServicePool.hpp>

namespace wawo { namespace net {

	template <class _MyPeerT, class _ServicePeerT = _MyPeerT >
	class ServiceProvider_Abstract :
		public wawo::RefObject_Abstract
	{

	public:
		typedef _ServicePeerT ServicePeerT;

		typedef _MyPeerT MyPeerT;
		typedef typename MyPeerT::MyMessageT MyMessageT;

		typedef ServiceProvider_Abstract<_MyPeerT,_ServicePeerT> MyServiceProviderT;
		typedef ServicePool<MyServiceProviderT> ServicePoolT;

		struct PeerMessageCtx
		{
			WAWO_SHARED_PTR<ServicePoolT> services;

			WAWO_REF_PTR<_MyPeerT> peer;
			WAWO_SHARED_PTR<MyMessageT> message;
		};

	protected:
		WAWO_REF_PTR<ServicePeerT> m_peer; //for service proxy, we need this to send message out, and receive message

	public:
		ServiceProvider_Abstract( uint32_t const& id ):
			m_id(id)
		{
		}

		~ServiceProvider_Abstract() {}

		void AttachPeer( WAWO_REF_PTR<ServicePeerT> const& peer ) {
			m_peer = peer;
		}

		inline uint32_t const& GetId() const {
			return m_id;
		}

		virtual void WaitMessage( PeerMessageCtx const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) = 0;

		/*
		 * consideration:
			1, call local
			2, call remote
			3, should explicitly distinct local call and remote call ?

		 * send message to remote
		 *
		 * return wawo::OK if call success,
		 *
		 *
		 */
		//int Call( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& message_in, WAWO_SHARED_PTR<MyMessageT> const& mesage_out ) {
		//

		//}

		//int void RPC( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& message_in, WAWO_SHARED_PTR<MyMessageT> const& mesage_out ) {
		//	WAWO_ASSERT( m_peer != NULL );

		//	RPC rpc();

		//	rpc.Request();
		//}

	private:
		uint32_t m_id;
	};


	class RPC {

	private:
		wawo::thread::Condition m_condition;
		RPC(  )
		{

		}

		~RPC()
		{

		}

		int Request() {
			// set call back
			// do request
			// wait response
			// get response
			// return

			return wawo::OK;
		}
	};
}}



#ifdef WAWO_ENABLE_SERVICE_EXECUTOR
#include <wawo/task/Task.hpp>
namespace wawo { namespace net {
	template <class ServiceListenerType>
	class ReceiveTask:
		public wawo::task::Task_Abstract {

	private:
		typedef ServiceListenerType _MyServiceListenerType;
		typedef typename ServiceListenerType::MyPeerType _MyPeerType;
		typedef typename _MyPeerType::NetMessageType _MyNetMessageType;

		WAWO_REF_PTR<_MyServiceListenerType> m_server;
		WAWO_REF_PTR<_MyPeerType> m_client;
		WAWO_REF_PTR<_MyNetMessageType> m_incoming;
	public:
		explicit ReceiveTask( WAWO_REF_PTR<_MyServiceListenerType> const& server, WAWO_REF_PTR<_MyPeerType> const& client, WAWO_REF_PTR<_MyNetMessageType> const& incoming ):
			Task_Abstract(wawo::task::P_NORMAL),
			m_server(server),
			m_client(client),
			m_incoming(incoming)
		{
			WAWO_ASSERT( m_incoming->GetMessageType() == wawo::net::core::MT_SEND );
		}
		~ReceiveTask() {}
		bool Isset() {
			return (m_server!=NULL)&&(m_client != NULL)&& (m_incoming != NULL);
		}
		void Reset() {
			WAWO_ASSERT( m_server != NULL );
			WAWO_ASSERT( m_client != NULL );
			WAWO_ASSERT( m_incoming != NULL );

			m_server = NULL;
			m_client = NULL;
			m_incoming = NULL;
		}
		void Run() {
			WAWO_ASSERT( Isset() );
			m_server->OnReceive(m_client, m_incoming) ;
		}
		void Cancel() {
			//nothing to do now...
		}
	};

	template <class ServiceListenerType>
	class RequestTask:
		public wawo::task::Task_Abstract {
	private:
		typedef ServiceListenerType _MyServiceListenerType;
		typedef typename ServiceListenerType::MyPeerType _MyPeerType;
		typedef typename _MyPeerType::NetMessageType _MyNetMessageType;

		WAWO_REF_PTR<_MyServiceListenerType> m_server;
		WAWO_REF_PTR<_MyPeerType> m_client;
		WAWO_REF_PTR<_MyNetMessageType> m_incoming;
	public:
		explicit RequestTask( WAWO_REF_PTR<_MyServiceListenerType> const& server , WAWO_REF_PTR<_MyPeerType> const& client, WAWO_REF_PTR<_MyNetMessageType> const& incoming ):
			Task_Abstract(wawo::task::P_NORMAL),
			m_server(server),
			m_client(client),
			m_incoming(incoming)
		{
			WAWO_ASSERT( m_incoming->GetMessageType() == wawo::net::core::MT_REQUEST );
		}
		~RequestTask() {}
		bool Isset() {
			return (m_server!=NULL)&&(m_client != NULL)&& (m_incoming != NULL);
		}
		void Reset() {
			WAWO_ASSERT( m_server != NULL );
			WAWO_ASSERT( m_client != NULL );
			WAWO_ASSERT( m_incoming != NULL );

			m_server = NULL;
			m_client = NULL;
			m_incoming = NULL;
		}
		void Run() {
			WAWO_ASSERT( Isset() );
			m_server->OnRequest(m_client, m_incoming) ;
		}
		void Cancel() {
			//nothing to do now...
		}
	};

	template <class ServiceListenerType>
	class RespondTask:
		public wawo::task::Task_Abstract {
	private:
		typedef ServiceListenerType _MyServiceListenerType;
		typedef typename ServiceListenerType::MyPeerType _MyPeerType;
		typedef typename _MyPeerType::NetMessageType _MyNetMessageType;

		WAWO_REF_PTR<_MyServiceListenerType> m_server;
		WAWO_REF_PTR<_MyPeerType> m_client;
		WAWO_REF_PTR<_MyNetMessageType> m_incoming;
		WAWO_REF_PTR<_MyNetMessageType> m_original;
	public:
		explicit RespondTask( WAWO_REF_PTR<_MyServiceListenerType> const& server, WAWO_REF_PTR<_MyPeerType> const& client, WAWO_REF_PTR<_MyNetMessageType> const& incoming,WAWO_REF_PTR<_MyNetMessageType> const& original ):
			Task_Abstract(wawo::task::P_NORMAL),
			m_server(server),
			m_client(client),
			m_incoming(incoming),
			m_original(original)
		{
			WAWO_ASSERT( m_incoming->GetMessageType() == wawo::net::core::MT_RESPONSE );
			WAWO_ASSERT( m_original->GetMessageType() == wawo::net::core::MT_REQUEST );
		}
		~RespondTask() {}
		bool Isset() {
			return (m_server != NULL)&&(m_client != NULL)&& (m_incoming != NULL) && m_original != NULL;
		}
		void Reset() {
			WAWO_ASSERT( m_server != NULL );
			WAWO_ASSERT( m_client != NULL );
			WAWO_ASSERT( m_incoming != NULL );
			WAWO_ASSERT( m_original != NULL );

			m_server = NULL;
			m_client = NULL;
			m_incoming = NULL;
			m_original = NULL;
		}
		void Run() {
			WAWO_ASSERT( Isset() );
			m_server->OnRespond(m_client, m_incoming, m_original ) ;
		}
		void Cancel() {
			//nothing to do now...
		}
	};
}}
#endif

#endif
