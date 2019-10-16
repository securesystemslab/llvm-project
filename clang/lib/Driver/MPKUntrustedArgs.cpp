//===--- MPKUntrustedArgs.cpp - Arguments for MPKUntrusted ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "clang/Driver/MPKUntrustedArgs.h"
#include "ToolChains/CommonArgs.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <clang/Driver/ToolChain.h>

using namespace clang;
using namespace clang::driver;
using namespace llvm::opt;

namespace {
constexpr char MPKUntrustedInstrumentOption[] = "-fprofile-mpk";
} // namespace

MPKUntrustedArgs::MPKUntrustedArgs(const ToolChain &TC, const ArgList &Args) {
  const Driver &D = TC.getDriver();
  const llvm::Triple &Triple = TC.getTriple();
  if (Args.hasFlag(options::OPT_fprofile_mpk, options::OPT_fnoprofile_mpk,
                   false)) {
    if (Triple.getOS() == llvm::Triple::Linux ||
        Triple.getOS() == llvm::Triple::Fuchsia) {
      switch (Triple.getArch()) {
      case llvm::Triple::x86_64:
      case llvm::Triple::arm:
      case llvm::Triple::aarch64:
      case llvm::Triple::ppc64le:
      case llvm::Triple::mips:
      case llvm::Triple::mipsel:
      case llvm::Triple::mips64:
      case llvm::Triple::mips64el:
        break;
      default:
        D.Diag(diag::err_drv_clang_unsupported)
            << (std::string(MPKUntrustedInstrumentOption) + " on " +
                Triple.str());
      }
    } else if (Triple.getOS() == llvm::Triple::FreeBSD ||
               Triple.getOS() == llvm::Triple::OpenBSD ||
               Triple.getOS() == llvm::Triple::Darwin ||
               Triple.getOS() == llvm::Triple::NetBSD) {
      if (Triple.getArch() != llvm::Triple::x86_64) {
        D.Diag(diag::err_drv_clang_unsupported)
            << (std::string(MPKUntrustedInstrumentOption) + " on " +
                Triple.str());
      }
    } else {
      D.Diag(diag::err_drv_clang_unsupported)
          << (std::string(MPKUntrustedInstrumentOption) + " on " +
              Triple.str());
    }

    MPKUntrusted = true;
    MPKUntrustedRT = true;
  }

  for (const auto &Filename : Args.getAllArgValues(options::OPT_finstr_mpk)) {
    if (llvm::sys::fs::exists(Filename)) {
      MPKUntrusted = true;
      MPKProfilePath.push_back(Filename);
    } else
      D.Diag(clang::diag::err_drv_no_such_file) << Filename;
  }
}

void MPKUntrustedArgs::addArgs(const ToolChain &TC, const ArgList &Args,
                               ArgStringList &CmdArgs,
                               types::ID InputType) const {
  if (!MPKUntrusted)
    return;

  CmdArgs.push_back(MPKUntrustedInstrumentOption);

  if (MPKUntrustedRT) {
    CmdArgs.push_back("-mllvm");
    CmdArgs.push_back("-profile-mpk");
  }

  if (!MPKProfilePath.empty()) {
    CmdArgs.push_back("-mllvm");
    SmallString<64> MPKFaultListArg("-instr-mpk=");
    MPKFaultListArg += MPKProfilePath.front();
    CmdArgs.push_back(Args.MakeArgString(MPKFaultListArg));
  }
}
