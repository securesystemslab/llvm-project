#ifndef MPK_COMMON_H
#define MPK_COMMON_H

#include "sanitizer_common/sanitizer_common.h"
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>

#define PAGE_MPK 0
#define SINGLE_STEP 1

// Flag for controlling optional Stats tracking
//#define MPK_STATS
#ifdef MPK_STATS
#include <atomic>

// Pointer to the global array tracking number of faults per allocation site
extern std::atomic<uint64_t> *AllocSiteUseCounter;
extern std::atomic<uint64_t> allocHookCalls;
extern std::atomic<uint64_t> reallocHookCalls;
extern std::atomic<uint64_t> deallocHookCalls;
extern std::atomic<uint64_t> AllocSiteCount;
#endif

//#define MPK_ENABLE_LOGGING
#ifdef MPK_ENABLE_LOGGING
#define REPORT(...) __sanitizer::Report(__VA_ARGS__)
#else
#define REPORT(...)                                                            \
  do {                                                                         \
  } while (0)
#endif

#define SINGLE_REPORT(...) __sanitizer::Report(__VA_ARGS__)

#endif // MPK_COMMON_H
