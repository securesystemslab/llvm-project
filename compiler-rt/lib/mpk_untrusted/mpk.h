// mpk.h
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

#ifndef ALLOCATOR_MPK_H
#define ALLOCATOR_MPK_H

#include "mpk_common.h"
#include <cstddef>

namespace __mpk_untrusted {
#define HAS_MPK 1

#define PKEY_ENABLE_ACCESS 0x0
#define PKEY_DISABLE_ACCESS 0x1
#define PKEY_DISABLE_WRITE 0x2

#define INVALID_PKEY 0x16

/**
 * Wrapper for getting PKRU pointer from ucontext.
 *
 * @param ctxt Pointer to ucontext.
 * @return Pointer to PKRU register in ucontext.
 */
__uint32_t *pkru_ptr(void *ctxt);

/**
 * Gets the protection bits for key from PKRU register
 *
 * @param pkru The PKRU register from ucontext
 * @param key The protection key to check
 * @return the protection bits for key
 */
int pkey_get(__uint32_t *pkru, int key);

/**
 * Sets the protection bits for key in the PKRU register
 *
 * @param pkru The PKRU register from ucontext
 * @param pkey  the protection key to set the bits for
 * @param rights the Read/Write bits to set
 * @return -1 error, 0 success
 */
int pkey_set(__uint32_t *pkru, int key, unsigned int rights);

#define XSTATE_PKRU_BIT (9)
#define XSTATE_PKRU 0x200

int pkru_xstate_offset(void);
} // namespace __mpk_untrusted

#endif // ALLOCATOR_MPK_H
