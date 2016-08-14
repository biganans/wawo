#ifndef _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_
#define _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_

#include <wawo/net/core/Listener_Abstract.hpp>

namespace wawo { namespace net {

	template <class _PeerT>
	class ServiceProvider_Abstract :
		virtual public wawo::RefObject_Abstract
	{
	public:
		typedef _PeerT PeerT;
		typedef typename PeerT::MessageT MessageT;

	private:
		u32_t m_id;
	public:
		ServiceProvider_Abstract( u32_t const& id ):
			m_id(id)
		{
		}

		virtual ~ServiceProvider_Abstract() {}
		inline u32_t const& GetId() const { return m_id;}

		virtual void WaitMessage( WWRP<PeerT> const& peer, WWSP<MessageT> const& incoming ) = 0;
	};
}}
#endif