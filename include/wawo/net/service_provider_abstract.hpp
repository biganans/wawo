#ifndef _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_
#define _WAWO_NET_SERVICE_PROVIDER_ABSTRACT_HPP_

#include <wawo/net/listener_abstract.hpp>

namespace wawo { namespace net {

	template <class _peer_t>
	class service_provider_abstract :
		virtual public wawo::ref_base
	{
	public:
		typedef _peer_t peer_t;
		typedef typename peer_t::message_t message_t;

	private:
		u32_t m_id;
	public:
		service_provider_abstract( u32_t const& id ):
			m_id(id)
		{
		}

		virtual ~service_provider_abstract() {}
		inline u32_t const& get_id() const { return m_id;}

		virtual void wait_message( WWRP<peer_t> const& peer, WWSP<message_t> const& incoming ) = 0;
	};
}}
#endif