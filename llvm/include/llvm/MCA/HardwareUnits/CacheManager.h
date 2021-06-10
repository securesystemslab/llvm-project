#ifndef LLVM_MCA_HARDWAREUNITS_CACHEMANAGER_H
#define LLVM_MCA_HARDWAREUNITS_CACHEMANAGER_H
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MCA/Instruction.h"
#include "llvm/MCA/HardwareUnits/HardwareUnit.h"
#include "llvm/MCA/HardwareUnits/LSUnit.h"

namespace llvm {
namespace mca {
class MetadataRegistry;

struct CacheUnit {
  unsigned Size;

  unsigned Assoc;

  unsigned LineSize;

  unsigned NumSets;

  // Number of bits required to represent
  // the line size
  unsigned NumLineSizeBits;

  SmallVector<uint64_t, 4> Tags;

  Optional<unsigned> PenaltyCycles;

  // FIXME: Note that we really should use the cache
  // info provided by `TargetTransformInfo`. But in order
  // to retrieve a TTI, we not only need to link against a
  // bunch of libraries, but also need to provide a `Function`.
  // Which is absolutely not worth it for MCA.
  struct Config {
    unsigned Associate;
    unsigned Size;
    unsigned LineSize;
    unsigned CacheMissPenalty;

    Config()
      : Associate(1U),        // Direct mapped
        Size(4U * 1024U),     // 4KB
        LineSize(64U),        // 64 bytes
        CacheMissPenalty(0U)  // Number of penalty cycles when cache miss
        {}
  };

  CacheUnit(const Config &C);
};

class CacheManager : public HardwareUnit {
  // Owner of cache units
  std::unique_ptr<CacheUnit> L1DCache, L2DCache;

  MetadataRegistry &MDRegistry;

  Optional<MDMemoryAccess> getMemoryAccessMD(const InstRef &IR);

public:
  struct CacheAccessStatus {
    unsigned NumL1DMiss = 0U;
    unsigned NumL2DMiss = 0U;

    // True if it's a cache hit
    operator bool() const {
      return !NumL1DMiss && !NumL2DMiss;
    }
  };

  CacheManager(StringRef CacheConfigFile,
               MetadataRegistry &MDR);

  /// Return true if there is a cache miss
  void onInstructionIssued(const InstRef &IR, CacheAccessStatus &CAS);

  /// Return the number of penalty cycles for a specific kind of
  /// cache miss.
  unsigned getPenaltyCycles(const CacheAccessStatus &CAS);

  virtual ~CacheManager();
};
} // end namespace mca
} // end namespace llvm
#endif
