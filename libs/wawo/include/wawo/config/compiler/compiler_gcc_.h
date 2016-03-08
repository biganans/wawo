#ifndef _CONFIG_COMPILER_WAWO_COMPILER_GCC_H_
#define _CONFIG_COMPILER_WAWO_COMPILER_GCC_H_


#include <stdint.h>

namespace wawo { namespace basic_type {

	typedef ::int8_t				_INT8_		;
	typedef ::uint8_t				_UINT8_		;

	typedef ::int16_t				_INT16_		;
	typedef ::uint16_t				_UINT16_	;

	typedef ::int32_t				_INT32_		;
	typedef ::uint32_t				_UINT32_	;

	typedef ::int64_t				_INT64_		;
	typedef ::uint64_t				_UINT64_	;

}}

#define WAWO_TLS __thread


#endif
