#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Instrumentation.h"

using namespace llvm;

#define DEBUG_TYPE "mpk-remove-inline"

namespace {

class MpkRemoveInlineAttr : public ModulePass {
public:
  static char ID;

  explicit MpkRemoveInlineAttr() : ModulePass(ID) {
    initializeMpkRemoveInlineAttrPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "MpkRemoveInlineAttr"; }
  bool runOnModule(Module &M) override;
};

void removeNeverInlineAttr(Function *F) {
  if (F) {
    if (F->hasFnAttribute(Attribute::NoInline)) {
      F->removeFnAttr(Attribute::NoInline);
    }
    F->addFnAttr(Attribute::AlwaysInline);
  }
}

bool MpkRemoveInlineAttr::runOnModule(Module &M) {
  removeNeverInlineAttr(M.getFunction("__rust_alloc"));
  removeNeverInlineAttr(M.getFunction("__rust_alloc_zeroed"));
  removeNeverInlineAttr(M.getFunction("__rust_realloc"));
  removeNeverInlineAttr(M.getFunction("__rust_dealloc"));
  for (Function &F : M) {
    if (F.hasFnAttribute(Attribute::RustAllocator)) {
      removeNeverInlineAttr(&F);
    }
  }
  return true;
}

} // namespace

char MpkRemoveInlineAttr::ID = 0;
INITIALIZE_PASS(MpkRemoveInlineAttr, "mpk-remove-inline",
                "Remove never-inline attributes from __rust_alloc calls.",
                false, false)

ModulePass *llvm::createMpkRemoveInlineAttrPass() {
  return new MpkRemoveInlineAttr();
}
