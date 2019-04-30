#ifndef SIMDCSV_COMMON_DEFS_H
#define SIMDCSV_COMMON_DEFS_H

#include <cassert>

// the input buf should be readable up to buf + SIMDJSON_PADDING
#ifdef __AVX2__
#define SIMDCSV_PADDING  sizeof(__m256i)
#else
// this is a stopgap; there should be a better description of the
// main loop and its behavior that abstracts over this
#define SIMDCSV_PADDING  32
#endif


// Align to N-byte boundary
#define ROUNDUP_N(a, n) (((a) + ((n)-1)) & ~((n)-1))
#define ROUNDDOWN_N(a, n) ((a) & ~((n)-1))

#define ISALIGNED_N(ptr, n) (((uintptr_t)(ptr) & ((n)-1)) == 0)

#ifdef _MSC_VER


#define really_inline inline
#define never_inline __declspec(noinline)

#define UNUSED
#define WARN_UNUSED

#ifndef likely
#define likely(x) x
#endif
#ifndef unlikely
#define unlikely(x) x
#endif

#else

#define really_inline inline __attribute__((always_inline, unused))
#define never_inline inline __attribute__((noinline, unused))

#define UNUSED __attribute__((unused))
#define WARN_UNUSED __attribute__((warn_unused_result))

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#endif  // MSC_VER

#endif // SIMDJSON_COMMON_DEFS_H
