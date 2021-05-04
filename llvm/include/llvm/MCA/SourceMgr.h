//===--------------------- SourceMgr.h --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements class SourceMgr. Class SourceMgr abstracts the input
/// code sequence (a sequence of MCInst), and assings unique identifiers to
/// every instruction in the sequence.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_MCA_SOURCEMGR_H
#define LLVM_MCA_SOURCEMGR_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/MCA/Instruction.h"
#ifndef NDEBUG
#include "llvm/Support/Format.h"
#endif
#include <list>

namespace llvm {
namespace mca {

// MSVC >= 19.15, < 19.20 need to see the definition of class Instruction to
// prevent compiler error C2139 about intrinsic type trait '__is_assignable'.
typedef std::pair<unsigned, const Instruction &> SourceRef;

struct SourceMgrBase {
  using UniqueInst = std::unique_ptr<Instruction>;

  virtual unsigned size() const = 0;

  virtual bool hasNext() const = 0;

  virtual bool isEnd() const = 0;

  virtual SourceRef peekNext() const = 0;

  virtual void updateNext() = 0;

  virtual ~SourceMgrBase() {}
};

class CircularSourceMgr : public SourceMgrBase {
  ArrayRef<UniqueInst> Sequence;
  unsigned Current;
  const unsigned Iterations;
  static const unsigned DefaultIterations = 100;

public:
  CircularSourceMgr(ArrayRef<UniqueInst> S, unsigned Iter)
      : Sequence(S), Current(0U),
        Iterations(Iter ? Iter : DefaultIterations) {}

  unsigned size() const override { return Sequence.size(); }

  unsigned getNumIterations() const { return Iterations; }
  bool hasNext() const override {
    return Current < (Iterations * Sequence.size());
  }
  bool isEnd() const override { return !hasNext(); }

  SourceRef peekNext() const override {
    assert(hasNext() && "Already at end of sequence!");
    return SourceRef(Current, *Sequence[Current % Sequence.size()]);
  }

  void updateNext() override { ++Current; }

  using const_iterator = typename decltype(Sequence)::const_iterator;
  const_iterator begin() const { return Sequence.begin(); }
  const_iterator end() const { return Sequence.end(); }
};

/// Using CircularSourceMgr by default
using SourceMgr = CircularSourceMgr;

class IncrementalSourceMgr : public SourceMgrBase {
  // Owner of all mca::Instruction
  std::list<UniqueInst> InstStorage;

  std::list<Instruction*> Staging;

  // FIXME: What happen when this overflow?
  unsigned TotalCounter;

  bool EOS;

  llvm::function_ref<void(Instruction*)> InstFreedCallback;

#ifndef NDEBUG
  size_t MaxInstStorageSize = 0U;
#endif

public:
  IncrementalSourceMgr() : TotalCounter(0U), EOS(false) {}

  void setOnInstFreedCallback(decltype(InstFreedCallback) CB) {
    InstFreedCallback = CB;
  }

  unsigned size() const override {
    llvm_unreachable("Not applicable");
  }

  bool hasNext() const override {
    return !Staging.empty();
  }
  bool isEnd() const override {
    return EOS;
  }

  SourceRef peekNext() const override {
    assert(hasNext());
    return SourceRef(TotalCounter, *Staging.front());
  }

  // Add a new instruction
  void addInst(UniqueInst &&Inst) {
    InstStorage.emplace_back(std::move(Inst));
    Staging.push_back(InstStorage.back().get());
#ifndef NDEBUG
    MaxInstStorageSize = InstStorage.size();
#endif
  }

  // Recycle an instruction
  void addRecycledInst(Instruction *Inst) {
    Staging.push_back(Inst);
  }

  void updateNext() override {
    ++TotalCounter;
    Instruction *I = Staging.front();
    I->reset();
    Staging.erase(Staging.begin());

    if (InstFreedCallback)
      InstFreedCallback(I);
  }

  void endOfStream() { EOS = true; }

#ifndef NDEBUG
  void printStatistic(raw_ostream &OS) {
    if (MaxInstStorageSize <= TotalCounter) {
      auto Ratio = double(MaxInstStorageSize) / double(TotalCounter);
      OS << "Cache ratio = "
         << MaxInstStorageSize << " / " << TotalCounter
         << llvm::format(" (%.2f%%)", (1.0 - Ratio) * 100.0)
         << "\n";
    } else {
      OS << "Error: Number of created instructions "
         << "are more than number of issued instructions\n";
    }
  }
#else
  void printStatistic(raw_ostream &OS) {
    OS << "Statistic for IncrementalSourceMgr "
       << "is only available in debug build\n";
  }
#endif
};
} // namespace mca
} // namespace llvm

#endif // LLVM_MCA_SOURCEMGR_H
