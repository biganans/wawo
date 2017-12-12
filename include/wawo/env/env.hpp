#ifndef _WAWO_ENV_ENV_HPP_
#define _WAWO_ENV_ENV_HPP_

#include <wawo/core.hpp>
#include <wawo/singleton.hpp>

namespace wawo { namespace env {
	class env_impl;
}}

#if WAWO_ISWIN
	#include <wawo/env/env_impl_win32.hpp>
#elif WAWO_ISGNU
	#include <wawo/env/env_impl_gnu.hpp>
#else
	#error
#endif

namespace wawo { namespace env {

	class env: public wawo::singleton<env> {

	private:
		env_impl* m_impl;
	public:
		env() 
		{
			m_impl = new env_impl();
			WAWO_ALLOC_CHECK(m_impl, sizeof(env_impl));
		}

		~env() 
		{
			WAWO_DELETE( m_impl );
		}

		int GetLocalIpList( std::vector<wawo::net::socket_addr>& addrs ) {
			return m_impl->GetLocalIpList(addrs);
		}

		int GetLocalComputerName(len_cstr& name) {
			return m_impl->GetLocalComputerName(name);
		}
	};
}}

#define WAWO_ENV_INSTANCE (wawo::env::env::instance())
#endif
