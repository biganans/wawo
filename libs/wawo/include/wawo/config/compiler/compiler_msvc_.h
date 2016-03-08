#ifndef _CONFIG_COMPILER_WAWO_COMPILER_MSVC_H_
#define _CONFIG_COMPILER_WAWO_COMPILER_MSVC_H_

namespace wawo { namespace basic_type {

	typedef __int8						_INT8_		;
	typedef unsigned __int8				_UINT8_		;

	typedef __int16						_INT16_		;
	typedef unsigned __int16			_UINT16_	;

	typedef __int32						_INT32_		;
	typedef unsigned __int32			_UINT32_	;

	typedef __int64						_INT64_		;
	typedef unsigned __int64			_UINT64_	;

}}

#define WAWO_TLS __declspec(thread)

#endif