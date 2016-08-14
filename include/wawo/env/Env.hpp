#ifndef _WAWO_ENV_ENV_HPP_
#define _WAWO_ENV_ENV_HPP_

#include <wawo/core.h>
#include <wawo/Singleton.hpp>

namespace wawo { namespace env {
	class Env_Impl;
}}

#if WAWO_ISWIN
	#include <wawo/env/Env_Impl_win32.hpp>
#elif WAWO_ISGNU
	#include <wawo/env/Env_Impl_gnu.hpp>
#else
	#error
#endif

namespace wawo { namespace env {

	class Env: public wawo::Singleton<Env> {

	private:
		Env_Impl* m_impl;
	public:
		Env() 
		{
			m_impl = new Env_Impl();
			WAWO_NULL_POINT_CHECK(m_impl);
		}
		~Env() 
		{
			WAWO_DELETE( m_impl );
		}

		int GetLocalIpList( std::vector<wawo::net::AddrInfo>& addrs ) {
			return m_impl->GetLocalIpList(addrs);
		}

		int GetLocalComputerName(Len_CStr& name) {
			return m_impl->GetLocalComputerName(name);
		}
	};
}}

#define WAWO_ENV_INSTANCE (wawo::env::Env::GetInstance())
#endif
