#ifndef _WAWO_NET_PEER_ABSTRACT_HPP
#define _WAWO_NET_PEER_ABSTRACT_HPP

#include <vector>
#include <wawo/SmartPtr.hpp>
#include <wawo/net/Socket.hpp>

namespace wawo { namespace net {

	class Peer_Abstract {

	protected:
		void* m_context;

	public:
		explicit Peer_Abstract():
			m_context(NULL)
		{
		}

		/*
		* u must detach all your sockets before peer's destruction call
		*/
		virtual ~Peer_Abstract() {}

		template <class CTXT>
		inline CTXT* GetContext() const { return static_cast<CTXT*>(m_context); }

		template <class CTXT>
		inline void SetContext(CTXT* const ctx) { m_context = static_cast<void*>(ctx); }

		virtual void Tick() {}

		virtual void OnEvent( WWRP<SocketEvent> const& evt ) = 0;

		virtual void AttachSocket( WWRP<Socket> const& socket ) = 0;
		virtual void DetachSocket( WWRP<Socket> const& socket ) = 0;
		virtual void GetSockets( std::vector< WWRP<Socket> >& sockets ) = 0;
		virtual bool HaveSocket(WWRP<Socket> const& socket) = 0;

		virtual int Close( int const& code = 0) = 0;
	};

}}
#endif