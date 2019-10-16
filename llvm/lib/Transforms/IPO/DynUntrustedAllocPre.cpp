//===-- UntrustedAlloc.cpp - UntrustedAlloc Infrastructure ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the common initialization infrastructure for the
// DynUntrustedAlloc library.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/DynUntrustedAllocPre.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <fstream>
#include <set>
#include <string>

#define DEBUG_TYPE "dyn-untrusted"
#define MPK_STATS

using namespace llvm;

namespace {
#ifdef MPK_STATS
// Tracker to count number of hook calls we create.
uint64_t hook_count = 0;

// Counters for tracking each type of hook separately
uint64_t alloc_hook_counter = 0;
uint64_t realloc_hook_counter = 0;
uint64_t dealloc_hook_counter = 0;
#endif

ConstantInt *getDummyID(Module &M) {
  return llvm::ConstantInt::get(IntegerType::getInt64Ty(M.getContext()), -1);
}

llvm::ConstantPointerNull *GlobalNullStr;

class DynUntrustedAllocPre : public ModulePass {
public:
  static char ID;

  DynUntrustedAllocPre() : ModulePass(ID) {
    initializeDynUntrustedAllocPrePass(*PassRegistry::getPassRegistry());
  }
  virtual ~DynUntrustedAllocPre() = default;

  bool runOnModule(Module &M) override {
    // Pre-inline pass:
    // Adds function hooks with dummy UniqueIDs immediately after calls
    // to __rust_alloc* functions. Additionally, we must remove the
    // NoInline attribute from RustAlloc functions.
    rust_alloc = M.getFunction("__rust_alloc");
    rust_alloc_zeroed = M.getFunction("__rust_alloc_zeroed");
    rust_realloc = M.getFunction("__rust_realloc");
    rust_dealloc = M.getFunction("__rust_dealloc");
    if (rust_alloc == nullptr &&
        rust_alloc_zeroed == nullptr &&
        rust_realloc == nullptr &&
        rust_dealloc == nullptr) {
      // If none of the functions we want to hook are present, skip module.
      return true;
    }
    
    
    GlobalNullStr = llvm::ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));

    AttrBuilder attrBldr;
    attrBldr.addAttribute(Attribute::NoUnwind);
    attrBldr.addAttribute(Attribute::ArgMemOnly);

    AttributeList fnAttrs = AttributeList::get(
        M.getContext(), AttributeList::FunctionIndex, attrBldr);

    auto rust_untrusted_alloc = M.getFunction("__rust_untrusted_alloc");
    if (rust_alloc != nullptr && !rust_untrusted_alloc) {
      Constant *r_untrusted_alloc = M.getOrInsertFunction(
          "__rust_untrusted_alloc", fnAttrs, Type::getInt8PtrTy(M.getContext()),
          IntegerType::getInt64Ty(M.getContext()),
          IntegerType::getInt64Ty(M.getContext()));
      rust_untrusted_alloc = cast<Function>(r_untrusted_alloc);
      rust_untrusted_alloc->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
    }

    auto rust_untrusted_alloc_zeroed = M.getFunction("__rust_untrusted_alloc_zeroed");
    if (rust_alloc_zeroed != nullptr && !rust_untrusted_alloc_zeroed) {
      Constant *r_untrusted_alloc_zeroed = M.getOrInsertFunction(
          "__rust_untrusted_alloc_zeroed", fnAttrs, Type::getInt8PtrTy(M.getContext()),
          IntegerType::getInt64Ty(M.getContext()),
          IntegerType::getInt64Ty(M.getContext()));
      rust_untrusted_alloc_zeroed = cast<Function>(r_untrusted_alloc_zeroed);
      rust_untrusted_alloc_zeroed->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
    }

    // Make function hook to add to all functions we wish to track
    Constant *allocHookFunc = M.getOrInsertFunction(
        "allocHook", fnAttrs, Type::getVoidTy(M.getContext()),
        Type::getInt8PtrTy(M.getContext()),
        IntegerType::get(M.getContext(), 64),
        IntegerType::getInt64Ty(M.getContext()),
        Type::getInt8PtrTy(M.getContext()),
        Type::getInt8PtrTy(M.getContext()));
    allocHook = cast<Function>(allocHookFunc);
    // set its linkage
    allocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

    Constant *reallocHookFunc = M.getOrInsertFunction(
        "reallocHook", fnAttrs, Type::getVoidTy(M.getContext()),
        Type::getInt8PtrTy(M.getContext()),
        IntegerType::get(M.getContext(), 64),
        Type::getInt8PtrTy(M.getContext()),
        IntegerType::get(M.getContext(), 64),
        IntegerType::getInt64Ty(M.getContext()),
        Type::getInt8PtrTy(M.getContext()),
        Type::getInt8PtrTy(M.getContext()));
    reallocHook = cast<Function>(reallocHookFunc);
    reallocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

    Constant *deallocHookFunc = M.getOrInsertFunction(
        "deallocHook", fnAttrs, Type::getVoidTy(M.getContext()),
        Type::getInt8PtrTy(M.getContext()),
        IntegerType::get(M.getContext(), 64),
        IntegerType::getInt64Ty(M.getContext()));
    deallocHook = cast<Function>(deallocHookFunc);
    deallocHook->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);

    hookFunctions(M);

    // Remove inline attribute from functions for inlining.
    removeInlineAttr(M);

#ifdef MPK_STATS
    // Print stats.
    printStats(M);
#endif
    LLVM_DEBUG(errs() << "Finished DynUntrustedPre.\n");
    return true;
  }

  Instruction *getHookInst(Module &M, CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (!F)
      return nullptr;

    if ((F == rust_alloc) ||
        (F == rust_alloc_zeroed)) {
#ifdef MPK_STATS
      alloc_hook_counter++;
#endif
      return CallInst::Create(
          (Function *)allocHook,
          {CS->getInstruction(), CS->getArgument(0), getDummyID(M), 
           GlobalNullStr, GlobalNullStr});
    } else if (F == rust_realloc) {
#ifdef MPK_STATS
      realloc_hook_counter++;
#endif
      return CallInst::Create((Function *)reallocHook,
                              {CS->getInstruction(), CS->getArgument(3),
                               CS->getArgument(0), CS->getArgument(1),
                               getDummyID(M), GlobalNullStr, GlobalNullStr});
    } else if (F == rust_dealloc) {
#ifdef MPK_STATS
      dealloc_hook_counter++;
#endif
      return CallInst::Create(
          (Function *)deallocHook,
          {CS->getArgument(0), CS->getArgument(1), getDummyID(M)});
    } else {
      return nullptr;
    }
  }

  void hookFunctions(Module &M) {
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      ReversePostOrderTraversal<Function *> RPOT(&F);

      for (BasicBlock *BB : RPOT) {
        for (Instruction &I : *BB) {
          CallSite CS(&I);
          if (!CS)
            continue;

          Instruction *newHook = getHookInst(M, &CS);
          if (!newHook)
            continue;

          BasicBlock::iterator NextInst;
          if (auto call = dyn_cast<CallInst>(&I)) {
            NextInst = ++I.getIterator();
            assert(NextInst != I.getParent()->end());
            LLVM_DEBUG(errs() << "CallInst(" << I << ") found next iterator: "
                              << *NextInst << "\n");
          } else if (auto invoke = dyn_cast<InvokeInst>(&I)) {
            BasicBlock *NormalDest = invoke->getNormalDest();
            if (!NormalDest->getSinglePredecessor()) {
              DominatorTree &DT =
                  getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
              auto BBNew = SplitEdge(invoke->getParent(), NormalDest, &DT);
              NextInst = BBNew->front().getIterator();
              LLVM_DEBUG(errs() << "InvokeInst(" << I
                                << ") with SplitEdge, found next iterator: "
                                << *NextInst << "\n");
            } else {
              NextInst = NormalDest->getFirstInsertionPt();
              assert(NextInst != NormalDest->end() &&
                     "Could not find insertion point for invoke instr");
              LLVM_DEBUG(errs() << "InvokeInst(" << I
                                << ") with single Pred, found next iterator: "
                                << *NextInst << "\n");
            }
          } else {
            continue;
          }

          IRBuilder<> IRB(&*NextInst);
          IRB.Insert(newHook);
#ifdef MPK_STATS
          ++hook_count;
#endif
        }
      }
    }
  }

#ifdef MPK_STATS
  void printStats(Module &M) {
    std::string TestDirectory = "TestResults";
    if (!llvm::sys::fs::is_directory(TestDirectory))
      llvm::sys::fs::create_directory(TestDirectory);

    llvm::Expected<llvm::sys::fs::TempFile> PreStats =
        llvm::sys::fs::TempFile::create(TestDirectory +
                                        "/static-pre-%%%%%%%.stat");
    if (!PreStats) {
      LLVM_DEBUG(errs() << "Error making unique filename: "
                        << llvm::toString(PreStats.takeError()) << "\n");
      return;
    }

    llvm::raw_fd_ostream OS(PreStats->FD, /* shouldClose */ false);
    OS << "Total number of hook instructions: " << hook_count << "\n"
       << "Number of alloc hook instructions: " << alloc_hook_counter << "\n"
       << "Number of realloc hook instructions: " << realloc_hook_counter
       << "\n"
       << "Number of dealloc hook instructions: " << dealloc_hook_counter
       << "\n";
    OS.flush();

    if (auto E = PreStats->keep()) {
      LLVM_DEBUG(errs() << "Error keeping pre-stats file: "
                        << llvm::toString(std::move(E)) << "\n");
      return;
    }
  }
#endif

  /// Iterate all Functions of Module M, remove NoInline attribute from
  /// Functions with RustAllocator attribute.
  void removeInlineAttr(Module &M) {
    
    auto rust_untrusted_alloc = M.getFunction("__rust_untrusted_alloc");
    if (rust_alloc != nullptr && !rust_untrusted_alloc) {
      errs() << "Module: " << M.getName() << "has __rust_alloc, but is missing __rust_untrusted_alloc.\n";
      assert(rust_untrusted_alloc != nullptr && "Missing rust_untrusted_alloc.");
    }
    auto rust_untrusted_alloc_zeroed =
        M.getFunction("__rust_untrusted_alloc_zeroed");
    if (rust_alloc_zeroed != nullptr && !rust_untrusted_alloc_zeroed) {
      errs() << "Module: " << M.getName() << "has __rust_alloc_zeroed, but is missing __rust_untrusted_alloc_zeroed.\n";
      assert(rust_untrusted_alloc_zeroed != nullptr && "Missing rust_untrusted_alloc_zeroed.");
    }
    if (rust_untrusted_alloc)
      rust_untrusted_alloc->setLinkage(
          llvm::GlobalValue::LinkageTypes::ExternalLinkage);
    if (rust_untrusted_alloc_zeroed) {
      rust_untrusted_alloc_zeroed->setLinkage(
        llvm::GlobalValue::LinkageTypes::ExternalLinkage);
    }

    
    
   
    for (Function &F : M) {
      if (F.hasFnAttribute(Attribute::RustAllocator)) {
        // Dont inline any functions that may be altered or hooked in the
        // PostPass
        if (&F == rust_alloc || &F == rust_alloc_zeroed || &F == rust_realloc ||
            &F == rust_dealloc || &F == rust_untrusted_alloc ||
            &F == rust_untrusted_alloc_zeroed)
          continue;
        F.removeFnAttr(Attribute::NoInline);
      }
    }

    if (rust_alloc) {
      rust_alloc->addFnAttr(Attribute::NoInline);
      rust_alloc->addFnAttr(Attribute::RustAllocator);
    }
    if (rust_alloc_zeroed) {
      rust_alloc_zeroed->addFnAttr(Attribute::NoInline);
      rust_alloc_zeroed->addFnAttr(Attribute::RustAllocator);
    }
    if (rust_realloc) {
      rust_realloc->addFnAttr(Attribute::NoInline);
      rust_realloc->addFnAttr(Attribute::RustAllocator);
    }
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<CallGraphWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
  }

private:
  Function *allocHook;
  Function *reallocHook;
  Function *deallocHook;
  Function *rust_alloc;
  Function *rust_alloc_zeroed;
  Function *rust_realloc;
  Function *rust_dealloc;
};

char DynUntrustedAllocPre::ID = 0;
} // namespace

INITIALIZE_PASS_BEGIN(DynUntrustedAllocPre, "dyn-untrusted-pre",
                      "DynUntrustedAlloc: Patch allocation sites with dynamic "
                      "function hooks for tracking allocation IDs.",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(DynUntrustedAllocPre, "dyn-untrusted-pre",
                    "DynUntrustedAlloc: Patch allocation sites with dynamic "
                    "function hooks for tracking allocation IDs.",
                    false, false)

ModulePass *llvm::createDynUntrustedAllocPrePass() {
  return new DynUntrustedAllocPre();
}

PreservedAnalyses DynUntrustedAllocPrePass::run(Module &M,
                                                ModuleAnalysisManager &AM) {
  DynUntrustedAllocPre dyn;
  if (!dyn.runOnModule(M)) {
    return PreservedAnalyses::all();
  }

  return PreservedAnalyses::none();
}
