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
// Pre passes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H
#define LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {
class ModulePass;

/// Pass to identify and add runtime hooks to all Rust alloc, realloc, and
/// dealloc calls. Additionally removes the NoInline attribute from functions
/// with the RustAllocator attribute.
class DynUntrustedAllocPrePass
    : public PassInfoMixin<DynUntrustedAllocPrePass> {
public:
  DynUntrustedAllocPrePass() {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

ModulePass *createDynUntrustedAllocPrePass();

void initializeDynUntrustedAllocPrePass(PassRegistry &Registry);
} // namespace llvm

#endif // LLVM_TRANSFORMS_DYNAMIC_MPK_UNTRUSTED_PRE_H
