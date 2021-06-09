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

unsigned CacheManager::getPenaltyCycles(CacheAccessStatus CAS) {
  unsigned Cycles = 0U;
  if (CAS & CAS_L1D_Miss) {
    if (auto MaybePenalty = L1DCache->PenaltyCycles)
      Cycles += *MaybePenalty;
  }
  if (CAS & CAS_L2D_Miss) {
    if (auto MaybePenalty = L2DCache->PenaltyCycles)
      Cycles += *MaybePenalty;
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

static bool onCacheSetRef(CacheUnit &Cache, uint32_t SetIdx, uint64_t Tag) {
  const unsigned TagIdx = SetIdx * Cache.Assoc;
  auto getSet = [&](unsigned Idx) -> uint64_t& {
    assert(TagIdx + Idx < Cache.Tags.size());
    return Cache.Tags[TagIdx + Idx];
  };

  for (int i = 0; i < int(Cache.Assoc); ++i) {
    if (Tag == getSet(i)) {
      for (int j = i; j > 0; --j)
        getSet(j) = getSet(j - 1);

      getSet(0) = Tag;
      return false;
    }
  }

  for (int j = Cache.Assoc - 1; j > 0; --j)
    getSet(j) = getSet(j - 1);

  getSet(0) = Tag;

  return true;
}

static bool onCacheRef(const MDMemoryAccess &MDA, CacheUnit &Cache) {
  const uint64_t Addr = MDA.Addr;
  const unsigned Size = MDA.Size;

  const uint64_t Block1 = Addr >> Cache.NumLineSizeBits,
                 Block2 = (Addr + Size - 1) >> Cache.NumLineSizeBits;
  const uint32_t Set1 = uint32_t(Block1 & (Cache.NumSets - 1));

  const uint64_t Tag1 = Block1;

  if (Block1 == Block2)
    return onCacheSetRef(Cache, Set1, Tag1);

  if (Block1 + 1 == Block2) {
    const uint32_t Set2 = uint32_t(Block2 & (Cache.NumSets - 1));
    const uint64_t Tag2 = Block2;
    if (onCacheSetRef(Cache, Set1, Tag1)) {
      onCacheSetRef(Cache, Set2, Tag2);
      return true;
    }
    return onCacheSetRef(Cache, Set2, Tag2);
  }

  WithColor::warning() << "Cache access straddles across two cache sets\n";

  return true;
}

CacheManager::CacheAccessStatus
CacheManager::onInstructionIssued(const InstRef &IR) {
  CacheAccessStatus CAS = CAS_L1D_Hit;
  if (auto MaybeMDA = getMemoryAccessMD(IR)) {
    ++NumDCacheAccesses;
    if (onCacheRef(*MaybeMDA, *L1DCache)) {
      CAS = CAS_L1D_Miss;
      ++NumL1DCacheMisses;
      if (onCacheRef(*MaybeMDA, *L2DCache)) {
        ++NumL2DCacheMisses;
        CAS |= CAS_L2D_Miss;
      }
    }
  }

  return CAS;
}
