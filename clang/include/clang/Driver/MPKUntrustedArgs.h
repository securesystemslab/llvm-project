//===--- MPKUntrustedArgs.h - Arguments for MPKUntrusted ----------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------------------===//
#ifndef LLVM_CLANG_DRIVER_MPKUNTRUSTEDARGS_H
#define LLVM_CLANG_DRIVER_MPKUNTRUSTEDARGS_H

#include "clang/Driver/Types.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"

#include <llvm/Option/Option.h>
#include <string>
#include <vector>

namespace clang {
namespace driver {

class ToolChain;

class MPKUntrustedArgs {
  bool MPKUntrusted = false;
  bool MPKUntrustedRT = false;
  std::vector<std::string> MPKProfilePath;

public:
  /// Parses the MPKUntrusted arguments from an argument list.
  MPKUntrustedArgs(const ToolChain &TC, const llvm::opt::ArgList &Args);
  void addArgs(const ToolChain &TC, const llvm::opt::ArgList &Args,
               llvm::opt::ArgStringList &CmdArgs, types::ID InputType) const;

  // Checks if the MPKUntrusted Runtime is requried.
  bool needsMPKUntrustedRt() const { return MPKUntrusted && MPKUntrustedRT; }
};

} // namespace driver
} // namespace clang

#endif // LLVM_CLANG_DRIVER_SYRINGEARGS_H
