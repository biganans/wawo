#ifndef _WAWO_NET_CREDENTIAL_HPP_
#define _WAWO_NET_CREDENTIAL_HPP_

#include <wawo/basic.hpp>
#include <wawo/string.hpp>

namespace wawo { namespace net {

	class Credential {

	private:
		wawo::Len_CStr m_name;
		wawo::Len_CStr m_password;

	public:
		Credential( wawo::Len_CStr const& name = wawo::Len_CStr("anonymous"), wawo::Len_CStr const& pwd = wawo::Len_CStr("") ): 
			m_name( name ),m_password(pwd) {}

		inline wawo::Len_CStr const& GetName() {
			return m_name;
		}

		inline wawo::Len_CStr const& GetName() const {
			return m_name;
		}

		inline wawo::Len_CStr const& GetPassword() {
			return m_password;
		}
		inline wawo::Len_CStr const& GetPassword() const {
			return m_password;
		}

		bool operator == ( Credential const& other ) {
			return m_name == other.m_name ;
		}

		bool operator == (Credential const& other) const {
			return m_name == other.m_name ;
		}
	};
}}
#endif