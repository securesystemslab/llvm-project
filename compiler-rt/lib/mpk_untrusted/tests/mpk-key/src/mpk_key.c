#define _GNU_SOURCE
#define HAS_MPK 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline void __wrpkru(unsigned int pkru) {
  unsigned int eax = pkru;
  unsigned int ecx = 0;
  unsigned int edx = 0;
  asm volatile(".byte 0x0f,0x01,0xef\n\t" : : "a"(eax), "c"(ecx), "d"(edx));
}

int __pkey_set(int pkey, unsigned long rights, unsigned long flags) {
  unsigned int pkru = (rights << (2 * pkey));
  __wrpkru(pkru);
  return 1;
}

int __pkey_mprotect(void *ptr, size_t size, unsigned long orig_prot,
                    unsigned long pkey) {
  return syscall(SYS_pkey_mprotect, ptr, size, orig_prot, pkey);
}

int __pkey_alloc(void) { return syscall(SYS_pkey_alloc, 0, 0); }

int __pkey_free(unsigned long pkey) { return syscall(SYS_pkey_free, pkey); }

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main(void) {
  int status;
  int pkey;
  int *buffer;

  /*
   *Allocate one page of memory
   */
  buffer = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (buffer == MAP_FAILED)
    errExit("mmap");

  /*
   * Put some random data into the page (still OK to touch)
   */
  *buffer = __LINE__;
  printf("buffer contains: %d\n", *buffer);

  /*
   * Allocate a protection key:
   */
  int pkey0 = __pkey_alloc();
  printf("pkey0 = %d\n", pkey0);
  pkey = __pkey_alloc();
  printf("pkey = %d\n", pkey);
  if (pkey == -1)
    errExit("pkey_alloc");

  /*
   * Disable access to any memory with "pkey" set,
   * even though there is none right now
   */
  status = __pkey_set(pkey, PKEY_DISABLE_ACCESS, 0);
  if (!status)
    errExit("pkey_set");

  /*
   * Set the protection key on "buffer".
   * Note that it is still read/write as far as mprotect() is
   * concerned and the previous pkey_set() overrides it.
   */
  status = __pkey_mprotect(buffer, getpagesize(), PROT_READ | PROT_WRITE, pkey);
  if (status == -1)
    errExit("pkey_mprotect");
  printf("about to read buffer again...\n");

  /*
   * This will crash, because we have disallowed access
   */
  printf("buffer contains: %d\n", *buffer);
  status = __pkey_free(pkey);
  if (status == -1)
    errExit("pkey_free");
  exit(EXIT_SUCCESS);
}
