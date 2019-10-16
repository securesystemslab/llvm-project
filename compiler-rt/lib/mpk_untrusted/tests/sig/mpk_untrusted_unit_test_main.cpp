//===-- mpk_untrusted_unit_test_main.cpp ----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of MPK Untrusted.
//
//===----------------------------------------------------------------------===//
#include "gtest/gtest.h"

namespace __sanitizer {
bool ReexecDisabled() { return true; }
} // namespace __sanitizer

int main(int argc, char **argv) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
