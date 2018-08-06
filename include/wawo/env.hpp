#ifndef _WAWO_ENV_ENV_HPP_
#define _WAWO_ENV_ENV_HPP_

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>

namespace wawo { namespace env_impl {
	class env_impl;
}}

#if WAWO_ISWIN
	#include <wawo/env_impl/win32.hpp>
#elif WAWO_ISGNU
	#include <wawo/env_impl/gnu.hpp>
#else
	#error
#endif

namespace wawo {

	class env: public wawo::singleton<env> {

	private:
		WWSP<env_impl::env_impl> m_impl;
	public:
		env() 
		{
			m_impl = wawo::make_shared<env_impl::env_impl>();
			WAWO_ALLOC_CHECK(m_impl, sizeof(env_impl::env_impl));
		}

		~env() 
		{
		}

		int get_local_dns_server_list(std::vector<wawo::net::address>& addrs) {
			return m_impl->get_local_dns_server_list(addrs);
		}

		int get_local_ip_list( std::vector<wawo::net::address>& addrs ) {
			return m_impl->get_local_ip_list(addrs);
		}

		int get_local_computer_name(std::string& name) {
			return m_impl->get_local_computer_name(name);
		}
	};
}

#define WAWO_ENV_INSTANCE (wawo::env::instance())
#endif