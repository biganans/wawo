#ifndef _WAWO_CONFIG_COMPILER_COMPILER_GNUGCC_H_
#define _WAWO_CONFIG_COMPILER_COMPILER_GNUGCC_H_

#define WAWO_TLS __thread
#define _WW_NOEXCEPT noexcept


#define WAWO_LIKELY(x) __builtin_expect(!!(x), 1)
#define WAWO_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif