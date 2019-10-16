//===- Transforms/UntrustedAlloc.h - UntrustedAlloc passes ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the constructor for the Dynamic Untrusted Allocation
// Post passes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H
#define LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {
class ModulePass;

/// Pass to patch all hook instructions after the inliner has run with
/// UniqueIDs. When supplied with a patch list (in the format of JSON file)
/// from previous runs, it will also patch allocation sites to be
/// untrusted.
class DynUntrustedAllocPostPass
    : public PassInfoMixin<DynUntrustedAllocPostPass> {
public:
  DynUntrustedAllocPostPass(std::string mpk_profile_path = "",
                            bool remove_hooks = false)
      : MPKProfilePath(mpk_profile_path), RemoveHooks(remove_hooks) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

private:
  std::string MPKProfilePath;
  bool RemoveHooks;
};

ModulePass *createDynUntrustedAllocPostPass(std::string mpk_profile_path,
                                            bool remove_hooks);

void initializeDynUntrustedAllocPostPass(PassRegistry &Registry);
} // namespace llvm

#endif // LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_POST_H
