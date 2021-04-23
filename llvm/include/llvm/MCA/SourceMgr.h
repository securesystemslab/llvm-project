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
#include "llvm/ADT/SmallVector.h"
#include "llvm/MCA/Instruction.h"

namespace llvm {
namespace mca {

// MSVC >= 19.15, < 19.20 need to see the definition of class Instruction to
// prevent compiler error C2139 about intrinsic type trait '__is_assignable'.
typedef std::pair<unsigned, const Instruction &> SourceRef;

class SourceMgrBase {
protected:
  unsigned Current;

  SourceMgrBase(): Current(0U) {}

public:
  using UniqueInst = std::unique_ptr<Instruction>;

  virtual unsigned size() const = 0;

  virtual bool hasNext() const = 0;

  virtual bool isEnd() const = 0;

  virtual SourceRef peekNext() const = 0;

  void updateNext() { ++Current; }
};

template<class StorageT>
class SourceMgrImpl : public SourceMgrBase {
protected:
  StorageT Sequence;

  SourceMgrImpl() = default;

  SourceMgrImpl(StorageT S) : Sequence(S) {}

public:
  using const_iterator = typename StorageT::const_iterator;
  const_iterator begin() const { return Sequence.begin(); }
  const_iterator end() const { return Sequence.end(); }
};

class CircularSourceMgr
  : public SourceMgrImpl<ArrayRef<SourceMgrBase::UniqueInst>> {
  const unsigned Iterations;
  static const unsigned DefaultIterations = 100;

public:
  CircularSourceMgr(ArrayRef<UniqueInst> S, unsigned Iter)
      : SourceMgrImpl(S),
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
};

/// Using CircularSourceMgr by default
using SourceMgr = CircularSourceMgr;

class IncrementalSourceMgr
  : public SourceMgrImpl<SmallVector<SourceMgrBase::UniqueInst, 8>> {
  bool EOS;

public:
  IncrementalSourceMgr(): SourceMgrImpl(), EOS(false) {}

  unsigned size() const override { return Sequence.size(); }

  bool hasNext() const override {
    return Current < Sequence.size();
  }
  bool isEnd() const override {
    return EOS;
  }

  SourceRef peekNext() const override {
    assert(hasNext());
    return SourceRef(Current, *Sequence[Current]);
  }

  void addInst(UniqueInst &&Inst) {
    Sequence.emplace_back(std::move(Inst));
  }

  void endOfStream() { EOS = true; }
};
} // namespace mca
} // namespace llvm

#endif // LLVM_MCA_SOURCEMGR_H
