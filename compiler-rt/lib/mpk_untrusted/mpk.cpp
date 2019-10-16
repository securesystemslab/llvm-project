// mpk.cpp
//
// Copyright 2018 Paul Kirth
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// This file supplies a set of APIs similar or identical to those found in
// glibc-2.27 We mimic these for future compatibility with standard libraries.

// set to 1 if we can use the same implementation for pkey_mprotect as
// glibc-2.27

#include "mpk.h"

#include <cerrno>
#include <cstdio>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace __mpk_untrusted {
/* Return a pointer to the PKRU register. */
__uint32_t *pkru_ptr(void *ctxt) {
  ucontext_t *uctxt = (ucontext_t *)ctxt;
  fpregset_t fpregset = uctxt->uc_mcontext.fpregs;
  char *fpregs = (char *)fpregset;
  int pkru_offset = __mpk_untrusted::pkru_xstate_offset();
  return (__uint32_t *)(&fpregs[pkru_offset]);
}

int pkey_get(__uint32_t *pkru, int key) {
#if HAS_MPK
  if (key < 0 || key > 15) {
    errno = EINVAL;
    return -1;
  }
  return (*pkru >> (2 * key)) & 3;
#else
  return 0;
#endif
}

/* set the bits in pkru for key using rights */
int pkey_set(__uint32_t *pkru, int key, unsigned int rights) {
#if HAS_MPK
  if (key < 0 || key > 15 || rights > 3) {
    errno = EINVAL;
    return -1;
  }
  unsigned int mask = 3 << (2 * key);
  *pkru = (*pkru & ~mask) | (rights << (2 * key));
  return 0;
#endif
  return 0;
}

static inline void __cpuid(unsigned int *eax, unsigned int *ebx,
                           unsigned int *ecx, unsigned int *edx) {
  /* ecx is often an input as well as an output. */
  asm volatile("cpuid;"
               : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
               : "0"(*eax), "2"(*ecx));
}

int pkru_xstate_offset(void) {
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
  int xstate_offset = 0;
  int xstate_size = 0;
  unsigned long XSTATE_CPUID = 0xd;
  int leaf;
  /* assume that XSTATE_PKRU is set in XCR0 */
  leaf = XSTATE_PKRU_BIT;
  {
    eax = XSTATE_CPUID;
    ecx = leaf;
    __cpuid(&eax, &ebx, &ecx, &edx);
    if (leaf == XSTATE_PKRU_BIT) {
      xstate_offset = ebx;
      xstate_size = eax;
    }
  }
  if (xstate_size == 0) {
    REPORT("INFO : Could not find size/offset of PKRU in xsave state\n");
    return 0;
  }
  return xstate_offset;
}
} // namespace __mpk_untrusted
