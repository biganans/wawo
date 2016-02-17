#ifndef _WAWO_ENV_ENV_HPP_
#define _WAWO_ENV_ENV_HPP_

#include <wawo/core.h>
#include <wawo/SafeSingleton.hpp>

namespace wawo { namespace utils {
	class Env_Impl;
}}

#ifdef WAWO_PLATFORM_WIN
	#include <wawo/env/Env_impl_win32.hpp>
#elif defined (WAWO_PLATFORM_POSIX)
	#include <wawo/env/Env_impl_posix.hpp>
#else
	#error
#endif

namespace wawo { namespace utils {

	class Env: public wawo::SafeSingleton<Env> {

	private:
		Env_Impl* m_impl;
	public:
		Env() {
			m_impl = new Env_Impl();
			WAWO_NULL_POINT_CHECK(m_impl);
		}
		~Env() {
			WAWO_DELETE( m_impl );
		}

		int GetLocalIpList( std::vector<wawo::net::core::IpInfo>& ips ) {
			return m_impl->GetLocalIpList(ips);
		}
	};
}}

#define WAWO_ENV_INSTANCE (wawo::utils::Env::GetInstance())
#endif
