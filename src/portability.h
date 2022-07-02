#ifndef PORTABILITY_H
#define PORTABILITY_H

#ifdef _MSC_VER
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#include <iso646.h>
#include <cstdint>

static inline bool add_overflow(uint64_t value1, uint64_t value2, uint64_t *result) {
	return _addcarry_u64(0, value1, value2, reinterpret_cast<unsigned __int64 *>(result));
}

#pragma intrinsic(_umul128)
static inline bool mul_overflow(uint64_t value1, uint64_t value2, uint64_t *result) {
	uint64_t high;
	*result = _umul128(value1, value2, &high);
	return high;
}


static inline uint64_t trailingzeroes(uint64_t input_num) {
    return _tzcnt_u64(input_num);
}

static inline uint64_t leadingzeroes(uint64_t  input_num) {
    return _lzcnt_u64(input_num);
}

static inline int hamming(uint64_t input_num) {
#ifdef _WIN64  // highly recommended!!!
	return (int)__popcnt64(input_num);
#else  // if we must support 32-bit Windows
	return (int)(__popcnt((uint32_t)input_num) +
		__popcnt((uint32_t)(input_num >> 32)));
#endif
}

#else
#include <cstdint>
#include <cstdlib>

#if defined(__BMI2__) || defined(__POPCOUNT__) || defined(__AVX2__)
#include <x86intrin.h>
#endif

static inline bool add_overflow(uint64_t  value1, uint64_t  value2, uint64_t *result) {
	return __builtin_uaddll_overflow(value1, value2, (unsigned long long*)result);
}
static inline bool mul_overflow(uint64_t  value1, uint64_t  value2, uint64_t *result) {
	return __builtin_umulll_overflow(value1, value2, (unsigned long long *)result);
}

/* result might be undefined when input_num is zero */
static inline int trailingzeroes(uint64_t input_num) {
#ifdef __BMI2__
	return _tzcnt_u64(input_num);
#else
	return __builtin_ctzll(input_num);
#endif
}

/* result might be undefined when input_num is zero */
static inline int leadingzeroes(uint64_t  input_num) {
#ifdef __BMI2__
	return _lzcnt_u64(input_num);
#else
	return __builtin_clzll(input_num);
#endif
}

/* result might be undefined when input_num is zero */
static inline int hamming(uint64_t input_num) {
#ifdef __POPCOUNT__
	return _popcnt64(input_num);
#else
	return __builtin_popcountll(input_num);
#endif
}

#endif // _MSC_VER


#endif // _PORTABILITY_H
