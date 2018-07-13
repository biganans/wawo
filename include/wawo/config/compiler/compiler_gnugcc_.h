#ifndef _WAWO_CONFIG_COMPILER_COMPILER_GNUGCC_H_
#define _WAWO_CONFIG_COMPILER_COMPILER_GNUGCC_H_

#define __WAWO_TLS __thread
#define __WW_NOEXCEPT noexcept
#define __WW_FORCE_INLINE __attribute__((always_inline))


#define WAWO_LIKELY(x) __builtin_expect(!!(x), 1)
#define WAWO_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif