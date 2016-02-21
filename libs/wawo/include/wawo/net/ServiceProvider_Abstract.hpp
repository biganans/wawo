#ifndef _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_
#define _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_

#include <wawo/net/core/Listener_Abstract.hpp>
#include <wawo/net/Context.hpp>

namespace wawo { namespace net {

	template <class _MyPeerT>
	class ServiceProvider_Abstract :
		public wawo::RefObject_Abstract
	{

	public:
		typedef _MyPeerT MyPeerT;
		typedef typename MyPeerT::MyMessageT MyMessageT;
		typedef typename MyPeerT::MySocketT MySocketT;

		typedef typename _MyPeerT::MyBasePeerCtxT MyBasePeerCtxT;
		typedef typename _MyPeerT::MyBasePeerMessageCtxT MyBasePeerMessageCtxT;

		ServiceProvider_Abstract<MyPeerT>( uint32_t const& id ):
			m_id(id)
		{
		}

		~ServiceProvider_Abstract<MyPeerT>() {}

		inline uint32_t const& GetId() {
			return m_id;
		}
		inline uint32_t const& GetId() const {
			return m_id;
		}

		virtual void HandleMessage( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) = 0;

	private:
		uint32_t m_id;
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
