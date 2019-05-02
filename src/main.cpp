#include <unistd.h> // for getopt

#include <iostream>
#include <vector>

#include "common_defs.h"
#include "csv_defs.h"
#include "io_util.h"
#include "timing.h"
#include "mem_util.h"
#include "portability.h"
using namespace std;


struct ParsedCSV {
  uint32_t n_indexes{0};
  uint32_t *indexes; 
};

struct simd_input {
#ifdef __AVX2__
  __m256i lo;
  __m256i hi;
#elif defined(__ARM_NEON)
  uint8x16_t i0;
  uint8x16_t i1;
  uint8x16_t i2;
  uint8x16_t i3;
#else
#error "It's called SIMDcsv for a reason, bro"
#endif
};

really_inline simd_input fill_input(const uint8_t * ptr) {
  struct simd_input in;
#ifdef __AVX2__
  in.lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ptr + 0));
  in.hi = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ptr + 32));
#elif defined(__ARM_NEON)
  in.i0 = vld1q_u8(ptr + 0);
  in.i1 = vld1q_u8(ptr + 16);
  in.i2 = vld1q_u8(ptr + 32);
  in.i3 = vld1q_u8(ptr + 48);
#endif
  return in;
}

// a straightforward comparison of a mask against input. 5 uops; would be
// cheaper in AVX512.
really_inline uint64_t cmp_mask_against_input(simd_input in, uint8_t m) {
#ifdef __AVX2__
  const __m256i mask = _mm256_set1_epi8(m);
  __m256i cmp_res_0 = _mm256_cmpeq_epi8(in.lo, mask);
  uint64_t res_0 = static_cast<uint32_t>(_mm256_movemask_epi8(cmp_res_0));
  __m256i cmp_res_1 = _mm256_cmpeq_epi8(in.hi, mask);
  uint64_t res_1 = _mm256_movemask_epi8(cmp_res_1);
  return res_0 | (res_1 << 32);
#elif defined(__ARM_NEON)
  const uint8x16_t mask = vmovq_n_u8(m); 
  uint8x16_t cmp_res_0 = vceqq_u8(in.i0, mask); 
  uint8x16_t cmp_res_1 = vceqq_u8(in.i1, mask); 
  uint8x16_t cmp_res_2 = vceqq_u8(in.i2, mask); 
  uint8x16_t cmp_res_3 = vceqq_u8(in.i3, mask); 
  return neonmovemask_bulk(cmp_res_0, cmp_res_1, cmp_res_2, cmp_res_3);
#endif
}


// return the quote mask (which is a half-open mask that covers the first
// quote in a quote pair and everything in the quote pair) 
// We also update the prev_iter_inside_quote value to
// tell the next iteration whether we finished the final iteration inside a
// quote pair; if so, this  inverts our behavior of  whether we're inside
// quotes for the next iteration.

really_inline uint64_t find_quote_mask(simd_input in, uint64_t &prev_iter_inside_quote) {
  uint64_t quote_bits = cmp_mask_against_input(in, '"');

#ifdef __AVX2__
  uint64_t quote_mask = _mm_cvtsi128_si64(_mm_clmulepi64_si128(
      _mm_set_epi64x(0ULL, quote_bits), _mm_set1_epi8(0xFF), 0));
#elif defined(__ARM_NEON)
  uint64_t quote_mask = vmull_p64( -1ULL, quote_bits);
#endif
  quote_mask ^= prev_iter_inside_quote;

  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20,
  // John Regher from Utah U. says this is fine code
  prev_iter_inside_quote =
      static_cast<uint64_t>(static_cast<int64_t>(quote_mask) >> 63);
  return quote_mask;
}


// flatten out values in 'bits' assuming that they are are to have values of idx
// plus their position in the bitvector, and store these indexes at
// base_ptr[base] incrementing base as we go
// will potentially store extra values beyond end of valid bits, so base_ptr
// needs to be large enough to handle this
really_inline void flatten_bits(uint32_t *base_ptr, uint32_t &base,
                                uint32_t idx, uint64_t bits) {
  uint32_t cnt = hamming(bits);
  uint32_t next_base = base + cnt;
  while (bits != 0u) {
    base_ptr[base + 0] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 1] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 2] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 3] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 4] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 5] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 6] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[base + 7] = static_cast<uint32_t>(idx) + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base += 8;
  }
  base = next_base;
}

bool find_indexes(const uint8_t * buf, size_t len, ParsedCSV & pcsv) {
  // does the previous iteration end inside a double-quote pair?
  uint64_t prev_iter_inside_quote = 0ULL;  // either all zeros or all ones
  //uint64_t prev_iter_cr_end = 0ULL; 
  size_t lenminus64 = len < 64 ? 0 : len - 64;
  size_t idx = 0;
  uint32_t *base_ptr = pcsv.indexes;
  uint32_t base = 0;

  for (; idx < lenminus64; idx += 64) {
#ifndef _MSC_VER
    __builtin_prefetch(buf + idx + 128);
#endif
    simd_input in = fill_input(buf+idx);
    uint64_t quote_mask = find_quote_mask(in, prev_iter_inside_quote);
    uint64_t sep = cmp_mask_against_input(in, ',');
#ifdef CRLF
    uint64_t cr = cmp_mask_against_input(in, 0x0d);
    uint64_t cr_adjusted = (cr << 1) | prev_iter_cr_end;
    uint64_t lf = cmp_mask_against_input(in, 0x0a);
    uint64_t end = lf & cr_adjusted;
    prev_iter_cr_end = cr >> 63;
#else
    uint64_t end = cmp_mask_against_input(in, 0x0a);
#endif
    // note - a bit of a high-wire act here with quotes
    // we can't put something inside the quotes with the CR
    // then outside the quotes with LF so it's OK to "and off"
    // the quoted bits here. Some other quote convention would
    // need to be thought about carefully
    uint64_t field_sep = (end | sep) & ~quote_mask;

    flatten_bits(base_ptr, base, idx, field_sep);
  }
  pcsv.n_indexes = base;
  return true;
}

int main(int argc, char * argv[]) {
  int c; 
  bool verbose = false;
  bool dump = false;
  int iterations = 10;
  bool squash_counters = false; // unused.

  while ((c = getopt(argc, argv, "vdi:s")) != -1){
    switch (c) {
    case 'v':
      verbose = true;
      break;
    case 'd':
      dump = true;
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    case 's':
      squash_counters = true;
      break;
    }
  }
  if (optind >= argc) {
    cerr << "Usage: " << argv[0] << " <csvfile>" << endl;
    exit(1);
  }

  const char *filename = argv[optind];
  if (optind + 1 < argc) {
    cerr << "warning: ignoring everything after " << argv[optind + 1] << endl;
  }

  if (verbose) {
    cout << "[verbose] loading " << filename << endl;
  }
  std::basic_string_view<uint8_t> p;
  try {
    p = get_corpus(filename, CSV_PADDING);
  } catch (const std::exception &e) { // caught by reference to base
    std::cout << "Could not load the file " << filename << std::endl;
    return EXIT_FAILURE;
  }
  if (verbose) {
    cout << "[verbose] loaded " << filename << " (" << p.size() << " bytes)" << endl;
  }
#ifdef __linux__
  vector<int> evts;
  evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
  evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
  evts.push_back(PERF_COUNT_HW_BRANCH_MISSES);
  evts.push_back(PERF_COUNT_HW_CACHE_REFERENCES);
  evts.push_back(PERF_COUNT_HW_CACHE_MISSES);
  evts.push_back(PERF_COUNT_HW_REF_CPU_CYCLES);
#endif //__linux__

  ParsedCSV pcsv;
  pcsv.indexes = new (std::nothrow) uint32_t[p.size()]; // can't have more indexes than we have data
  if(pcsv.indexes == nullptr) {
    cerr << "You are running out of memory." << endl;
    return EXIT_FAILURE;
  }

#ifdef __linux__
  TimingAccumulator ta(2, evts);
#endif // __linux__
  double total = 0; // naive accumulator
  for (int i = 0; i < iterations; i++) {
      clock_t start = clock(); // brutally portable
#ifdef __linux__
      {TimingPhase p1(ta, 0);
#endif // __linux__
      find_indexes(p.data(), p.size(), pcsv);
#ifdef __linux__
      }{TimingPhase p2(ta, 1);} // the scoping business is an instance of C++ extreme programming
#endif // __linux__
      total += clock() - start; // brutally portable 
  }

  if (dump) {
    for (size_t i = 0; i < pcsv.n_indexes; i++) {
      cout << pcsv.indexes[i] << ": ";
      if (i != pcsv.n_indexes-1) {
        for (size_t j = pcsv.indexes[i]; j < pcsv.indexes[i+1]; j++) {
          cout << p[j];
        }
      }
      cout << "\n";
    }
  } 
  if(verbose) {
    cout << "number of indexes found    : " << pcsv.n_indexes << endl;
    cout << "number of bytes per index : " << p.size() / double(pcsv.n_indexes) << endl;
  }
  double volume = iterations * p.size();
  double time_in_s = total / CLOCKS_PER_SEC;
  if(verbose) {
    cout << "Total time in (s)          = " << time_in_s << endl;
    cout << "Number of iterations       = " << volume << endl;
  }
#ifdef __linux__
  if(verbose) {
    cout << "Number of cycles                   = " << ta.results[0] << endl;
    cout << "Number of cycles per byte          = " << ta.results[0] / volume << endl;
    cout << "Number of cycles (ref)             = " << ta.results[5] << endl;
    cout << "Number of cycles (ref) per byte    = " << ta.results[5] / volume << endl;
    cout << "Number of instructions             = " << ta.results[1] << endl;
    cout << "Number of instructions per byte    = " << ta.results[1] / volume << endl;
    cout << "Number of instructions per cycle   = " << double(ta.results[1]) / ta.results[0] << endl;
    cout << "Number of branch misses            = " << ta.results[2] << endl;
    cout << "Number of branch misses per byte   = " << ta.results[2] / volume << endl;
    cout << "Number of cache references         = " << ta.results[3] << endl;
    cout << "Number of cache references per b.  = " << ta.results[3] / volume << endl;
    cout << "Number of cache misses             = " << ta.results[4] << endl;
    cout << "Number of cache misses per byte    = " << ta.results[4] / volume << endl;
    cout << "CPU freq (effective)               = " << ta.results[0] / time_in_s / (1000 * 1000 * 1000) << endl; 
    cout << "CPU freq (base)                    = " << ta.results[5] / time_in_s / (1000 * 1000 * 1000) << endl; 
   } else {
    ta.dump();
  }
  cout << "Cycles per byte " << (1.0*ta.results[0])/volume << "\n";
#endif
  cout << " GB/s: " << volume / time_in_s / (1024 * 1024 * 1024) << endl;
  if (verbose) {
    cout << "[verbose] done " << endl;
  }
  delete[] pcsv.indexes;
  aligned_free((void*)p.data());
  return EXIT_SUCCESS;
}

