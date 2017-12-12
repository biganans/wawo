#ifndef _WAWO_CONFIG_COMPILER_COMPILER_MSVC_H_
#define _WAWO_CONFIG_COMPILER_COMPILER_MSVC_H_

#define WAWO_TLS __declspec(thread)


#if _MSC_VER == 1700

	#undef _WW_OVERRIDE
	#define _WW_OVERRIDE

	#undef _WW_NOEXCEPT
	#define _WW_NOEXCEPT throw()
	
	#undef _WW_CONSTEXPR
	#define _WW_CONSTEXPR

	#define _WW_NO_CXX11_DELETED_FUNC
	#define _WW_NO_CXX11_CLASS_TEMPLATE_DEFAULT_TYPE
	#define _WW_NO_CXX11_FUNCTION_TEMPLATE_DEFAULT_VALUE
	#define _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS

	#define _WW_NO_CXX11_LAMBDA_PASS_DEFAULT_VALUE
	#define _WW_STD_CHRONO_STEADY_CLOCK_EXTEND_SYSTEM_CLOCK
#else
	#define _WW_NOEXCEPT noexcept
#endif

#ifdef _WW_NO_CXX11_TEMPLATE_VARIADIC_ARGS
	#if defined(_VARIADIC_MAX) && (_VARIADIC_MAX != 10)
		#pragma message( __FILE__ " must be prior to include<memory>")
	#else
		#define _VARIADIC_MAX 10
	#endif
#endif

#define WAWO_LIKELY(x) (x)
#define WAWO_UNLIKELY(x) (x)

#endif