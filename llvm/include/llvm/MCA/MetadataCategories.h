#ifndef LLVM_MCA_METADATACATEGORIES_H
#define LLVM_MCA_METADATACATEGORIES_H
namespace llvm {
namespace mca {
// Metadata for LSUnit
static constexpr unsigned MD_LSUnit_MemAccess = 0;

// Used for marking the start of custom MD Category
static constexpr unsigned MD_LAST = MD_LSUnit_MemAccess;
} // end namespace mca
} // end namespace llvm
#endif
