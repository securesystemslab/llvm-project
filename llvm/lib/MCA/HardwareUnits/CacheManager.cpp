#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MCA/HardwareUnits/CacheManager.h"
#include "llvm/MCA/MetadataCategories.h"
#include "llvm/MCA/MetadataRegistry.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/WithColor.h"
#include <functional>

using namespace llvm;
using namespace mca;

#define DEBUG_TYPE "llvm-mca"

STATISTIC(NumDCacheAccesses, "Total number of data cache accesses");
STATISTIC(NumL1DCacheMisses, "Number of cache misses in L1 D$");
STATISTIC(NumL2DCacheMisses, "Number of cache misses in L2 D$");

CacheUnit::CacheUnit(const CacheUnit::Config &C)
  : Size(C.Size), Assoc(C.Associate), LineSize(C.LineSize),
    NumSets((Size / LineSize) / Assoc),
    NumLineSizeBits(llvm::Log2_32_Ceil(LineSize)),
    Tags(size_t(NumSets * Assoc), 0U),
    PenaltyCycles(Optional<unsigned>::create(C.CacheMissPenalty?
                                             &C.CacheMissPenalty : nullptr)) {}

CacheManager::~CacheManager() {}

namespace {
// A simple RAII that calls a function upon destroy
class Defer {
  std::function<void(void)> ExitCB;

public:
  Defer(std::function<void(void)> &&CB)
    : ExitCB(std::move(CB)) {}

  ~Defer() {
    ExitCB();
  }
};
} // end anonymous namespace

CacheManager::CacheManager(StringRef CacheConfigFile,
                           MetadataRegistry &MDR)
  : MDRegistry(MDR) {
  CacheUnit::Config L1DConfig, L2DConfig;
  Defer OnExitCallback([&,this] {
                         L1DCache.reset(new CacheUnit(L1DConfig));
                         L2DCache.reset(new CacheUnit(L2DConfig));
                       });

  // Try to parse the config file
  auto ErrOrBuffer = MemoryBuffer::getFile(CacheConfigFile, /*IsText=*/true);
  if (!ErrOrBuffer) {
    WithColor::error() << "Fail to read cache config file: "
                       << ErrOrBuffer.getError().message() << "\n";
    return;
  }
  MemoryBuffer &Buffer = *ErrOrBuffer.get();

  auto JsonOrErr = json::parse(Buffer.getBuffer());
  if (!JsonOrErr) {
    handleAllErrors(JsonOrErr.takeError(),
                    [](const ErrorInfoBase &E) {
                      E.log(WithColor::error() << "Fail to parse config file: ");
                      errs() << "\n";
                    });
    return;
  }
  const json::Value &RootJson = *JsonOrErr;
  const auto *RootObj = RootJson.getAsObject();
  if (!RootObj) {
    WithColor::error() << "Expecting an object at root\n";
    return;
  }

  auto populateCacheInfo = [](const json::Object &Entry,
                              CacheUnit::Config &C) {
    if (auto MaybeSize = Entry.getInteger("size"))
      C.Size = *MaybeSize;
    if (auto MaybeAssociate = Entry.getInteger("associate"))
      C.Associate = *MaybeAssociate;
    if (auto MaybeLineSize = Entry.getInteger("line_size"))
      C.LineSize = *MaybeLineSize;
    if (auto MaybeCacheMissPenalty = Entry.getInteger("penalty"))
      C.CacheMissPenalty = *MaybeCacheMissPenalty;
  };

  // Parse L1D entry
  if (const auto *L1DEntry = RootObj->getObject("l1d"))
    populateCacheInfo(*L1DEntry, L1DConfig);
  // Parse L2D entry
  if (const auto *L2DEntry = RootObj->getObject("l2d"))
    populateCacheInfo(*L2DEntry, L2DConfig);

  // The `Defer` RAII object will configure the cache objects upon exit
}

unsigned CacheManager::getPenaltyCycles(const CacheAccessStatus &CAS) {
  unsigned Cycles = 0U;
  if (CAS.NumL1DMiss) {
    if (auto MaybePenalty = L1DCache->PenaltyCycles)
      Cycles += *MaybePenalty * CAS.NumL1DMiss;
  }
  if (CAS.NumL2DMiss) {
    if (auto MaybePenalty = L2DCache->PenaltyCycles)
      Cycles += *MaybePenalty * CAS.NumL2DMiss;
  }

  return Cycles;
}

Optional<MDMemoryAccess> CacheManager::getMemoryAccessMD(const InstRef &IR) {
  if (auto MaybeMDTok = IR.getInstruction()->getMetadataToken()) {
    auto &MACat = MDRegistry[MD_LSUnit_MemAccess];
    return MACat.get<MDMemoryAccess>(*MaybeMDTok);
  }
  return llvm::None;
}

static bool cacheSetRef(CacheUnit &Cache, uint32_t SetIdx, uint64_t Tag) {
  const unsigned TagIdx = SetIdx * Cache.Assoc;
  auto getSet = [&](unsigned Idx) -> uint64_t& {
    assert(TagIdx + Idx < Cache.Tags.size());
    return Cache.Tags[TagIdx + Idx];
  };

  // The first item is the MRU
  if (Tag == getSet(0))
    return false;

  // If it's a tag other than MRU, set it as MRU and move reset
  // of the items down for one slot.
  for (int i = 1; i < int(Cache.Assoc); ++i) {
    if (Tag == getSet(i)) {
      for (int j = i; j > 0; --j)
        getSet(j) = getSet(j - 1);

      getSet(0) = Tag;
      return false;
    }
  }

  // Now this is a miss, so set it as MRU and move reset of the
  // items down for one slot.
  for (int j = Cache.Assoc - 1; j > 0; --j)
    getSet(j) = getSet(j - 1);

  getSet(0) = Tag;

  return true;
}

static bool onCacheRef(const MDMemoryAccess &MDA, CacheUnit &Cache) {
  const uint64_t Addr = MDA.Addr;
  const unsigned Size = MDA.Size;

  // A block has the size of a cache line.
  // First, calculate the range of affected blocks.
  const uint64_t FirstBlock = Addr >> Cache.NumLineSizeBits,
                 LastBlock = (Addr + Size - 1) >> Cache.NumLineSizeBits;
  const uint32_t FirstSet = uint32_t(FirstBlock & (Cache.NumSets - 1));

  // Usually real hardware will use
  // tag = block >> log2(# of sets)
  // as the tag, but using the entire block index is also fine.
  const uint64_t FirstTag = FirstBlock;

  // Access within a single cache line
  if (FirstBlock == LastBlock)
    return cacheSetRef(Cache, FirstSet, FirstTag);

  // Access across two cache lines
  if (FirstBlock + 1 == LastBlock) {
    const uint32_t LastSet = uint32_t(LastBlock & (Cache.NumSets - 1));
    const uint64_t LastTag = LastBlock;
    if (cacheSetRef(Cache, FirstSet, FirstTag)) {
      cacheSetRef(Cache, LastSet, LastTag);
      return true;
    }
    return cacheSetRef(Cache, LastSet, LastTag);
  }

  llvm_unreachable("Cache access straddles across two cache sets\n");
}

void CacheManager::onInstructionIssued(const InstRef &IR,
                                       CacheAccessStatus &CAS) {
  if (auto MaybeMDA = getMemoryAccessMD(IR)) {
    const auto *MDA = &*MaybeMDA;
    unsigned BMAIdx = 0U;
    while (true) {
      ++NumDCacheAccesses;
      if (onCacheRef(*MDA, *L1DCache)) {
        ++CAS.NumL1DMiss;
        ++NumL1DCacheMisses;
        if (onCacheRef(*MDA, *L2DCache)) {
          ++NumL2DCacheMisses;
          ++CAS.NumL2DMiss;
        }
      }
      // Check if there are bundled memory accesses
      if (auto &BMA = MDA->BundledMAs)
        if (BMAIdx < BMA->Accesses.size()) {
          MDA = &BMA->Accesses[BMAIdx++];
          continue;
        }

      break;
    }
  }
}
