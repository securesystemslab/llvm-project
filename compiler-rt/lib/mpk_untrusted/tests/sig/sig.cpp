// define a macro so that we can access register indexes from ucontext.h
#define _GNU_SOURCE
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

// TODO: This should not need a relative path, either use an include dir or
// relocate in lib
#include "../util/mpk_untrusted_test_config.h"
#include "gtest/gtest.h"

#define PAGE_SIZE 4096
#define TF 0x100
#define si_pkey_offset 0x20

// uses write directly
// only 128 char strings
void sig_printf(const char *format, ...) {
  char msg[128];
  va_list argp;
  va_start(argp, format);
  snprintf(&msg[0], 128, format, argp);
  write(STDOUT_FILENO, msg, strlen(msg));
}

void segv_handler(int signal, siginfo_t *si, void *vucontext) {
  sig_printf("Enter SEGV Handler\n");

  uint64_t mask = ~(PAGE_SIZE - 1);
  // Obtains pointer causing fault
  void *ptr = si->si_addr;
  uintptr_t ptr_val = (uintptr_t)ptr;

  sig_printf("ptr = %p\n", ptr);
  void *aligned_ptr = (void *)(ptr_val & mask);

  sig_printf("aligned_tpr = %p\n", aligned_ptr);
  mprotect(aligned_ptr, PAGE_SIZE, PROT_READ | PROT_WRITE);

  sig_printf("mprotect() done\n");
  // set trap flag

  ucontext_t *uctxt = (ucontext_t *)vucontext;
  // set trap flag on next instruction
  uctxt->uc_mcontext.gregs[REG_EFL] |= TF;
}

void trap_handler(int signal, siginfo_t *si, void *vucontext) {
  sig_printf("handling a trap!\n");
  ucontext_t *uctxt = (ucontext_t *)vucontext;
  // clear trap flag so we can restore pkru regiser
  uctxt->uc_mcontext.gregs[REG_EFL] &= ~TF;
}

TEST(SigHandler, SigTest) {
  struct sigaction sa_segv;

  sa_segv.sa_flags = SA_SIGINFO;
  sigemptyset(&sa_segv.sa_mask);
  sa_segv.sa_sigaction = segv_handler;
  ASSERT_NE(sigaction(SIGSEGV, &sa_segv, nullptr), -1)
      << "Failed to register sigaction for SIGSEGV.\n";

  struct sigaction sa_trap;

  sa_trap.sa_flags = SA_SIGINFO;
  sigemptyset(&sa_trap.sa_mask);
  sa_trap.sa_sigaction = trap_handler;
  ASSERT_NE(sigaction(SIGTRAP, &sa_trap, nullptr), -1)
      << "Failed to register sigaction for SIGTRAP.\n";

  char *ptr = (char *)mmap(nullptr, PAGE_SIZE, PROT_NONE,
                           MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  ASSERT_NE(ptr, MAP_FAILED) << "mmap failed.\n";
  strncpy(ptr, "hello world!", 1024);
  EXPECT_STREQ(ptr, "hello world!");
}
