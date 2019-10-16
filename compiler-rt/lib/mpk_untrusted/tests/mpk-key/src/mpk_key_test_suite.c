#define _GNU_SOURCE
#define HAS_MPK 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define PROT_PTRS 10

int pkey;
int8_t *prot_ptrs [PROT_PTRS] = {NULL};
int prot_count = 0;

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

int inc_prot() {
  if (prot_count < 10) {
    return prot_count++;
  } else {
    return prot_count;
  }
}

void dec_prot() {
  if (prot_count > 0) {
    prot_count--;
  }
}

int is_protected(int8_t *ptr) {
  for (int i = 0; i < prot_count; i++) {
    if (prot_ptrs[i] == ptr) {
      // return the key it is
      return i;
    }
  }
  return -1;
}

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int8_t *__rust_alloc(int64_t size, int64_t align) {
  /*
   *Allocate one page of memory
   */
  int8_t *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, align);
  if (buffer == MAP_FAILED)
    errExit("mmap");
  int status = __pkey_mprotect(buffer, size, PROT_READ | PROT_WRITE, pkey);
  if (status == -1)
    errExit("pkey_mprotect");
  prot_ptrs[inc_prot()] = buffer;
  return buffer;
}

// For testing purposes, there is no difference between zeroed and regular alloc.
int8_t *__rust_alloc_zeroed(int64_t size, int64_t align) {
  return __rust_alloc(size, align);
}

int8_t *__rust_untrusted_alloc(int64_t size, int64_t align) {
  /*
   *Allocate one page of memory
   */
  int8_t *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, align);
  if (buffer == MAP_FAILED)
    errExit("mmap");
  int status = mprotect(buffer, size, PROT_READ | PROT_WRITE);
  if (status == -1)
    errExit("pkey_mprotect");
  return buffer;
}

int8_t *__rust_untrusted_alloc_zeroed(int64_t size, int64_t align) {
  return __rust_untrusted_alloc(size, align);
}

int8_t *__rust_realloc(int8_t *old_ptr, int64_t old_size, int64_t align, int64_t new_size) {
  int8_t *buffer = mremap(old_ptr, old_size, new_size, MREMAP_MAYMOVE);
  if (buffer == MAP_FAILED)
    errExit("mmap");
  int res = is_protected(old_ptr);
  int status;
  if (res != -1) {
    if (buffer != old_ptr)
      prot_ptrs[res] = buffer;
    status = __pkey_mprotect(buffer, new_size, PROT_READ | PROT_WRITE, pkey);
  } else {
    status = mprotect(buffer, new_size, PROT_READ | PROT_WRITE);
  }
  if (status == -1)
    errExit("pkey_mprotect");
  return buffer;
}

void __rust_dealloc(int8_t *ptr, int64_t size, int64_t align) {
  int result = is_protected(ptr);
  if (result != -1)
    prot_ptrs[result] = NULL;
  if (result == prot_count)
    dec_prot();
  int status = munmap(ptr, size);
  if (status == -1)
    errExit("munmap");
}

void simple_set() {
  int8_t *buffer;
  /*
   *Allocate one page of memory
   */
  buffer = __rust_alloc(PAGE_SIZE, 0);
  printf("alloc buffer.\n");

  /*
   * Put some random data into the page
   */
  *buffer = __LINE__;
  printf("buffer contains: %d\n", *buffer);

  __rust_dealloc(buffer, PAGE_SIZE, 0);
  printf("dealloc buffer.\n");
}

void simple_zeroed_set() {
  int8_t *buffer;
  /*
   *Allocate one page of memory
   */
  buffer = __rust_alloc_zeroed(PAGE_SIZE, 0);
  printf("alloc zeroed buffer.\n");

  /*
   * Put some random data into the page
   */
  *buffer = __LINE__;
  printf("buffer contains: %d\n", *buffer);

  __rust_dealloc(buffer, PAGE_SIZE, 0);
  printf("dealloc zeroed buffer.\n");
}

void simple_realloc() {
  int8_t *buffer;
  buffer = __rust_alloc(PAGE_SIZE, 0);
  printf("alloc zeroed buffer.\n");

  *buffer = __LINE__;
  printf("buffer contains: %d\n", *buffer);

  buffer = __rust_realloc(buffer, PAGE_SIZE, 0, PAGE_SIZE*2);
  printf("buffer realloced.\n");

  *buffer = __LINE__;
  printf("buffer still contains: %d\n", *buffer);

  __rust_dealloc(buffer, PAGE_SIZE*2, 0);
  printf("dealloc realloc buffer.\n");
}

void complex_set() {
  time_t t;
  srand((unsigned) time(&t));

  if (rand() % 2) {
    int8_t *buffer;
    /*
    *Allocate one page of memory
    */
    buffer = __rust_alloc(PAGE_SIZE, 0);
    printf("alloc buffer.\n");

    /*
    * Put some random data into the page
    */
    *buffer = __LINE__;
    printf("buffer contains: %d\n", *buffer);

    __rust_dealloc(buffer, PAGE_SIZE, 0);
    printf("dealloc buffer.\n");
  }

  if (rand() % 2) {
    int8_t *buffer;
    /*
    *Allocate one page of memory
    */
    buffer = __rust_alloc(PAGE_SIZE, 0);
    printf("alloc buffer.\n");

    /*
    * Put some random data into the page
    */
    *buffer = __LINE__;
    printf("buffer contains: %d\n", *buffer);

    __rust_dealloc(buffer, PAGE_SIZE, 0);
    printf("dealloc buffer.\n");
  }

  if (rand() % 3) {
    simple_set();
  }

  if (rand() % 3) {
    simple_realloc();
  }
}

int main(void) {
  int status;
  int8_t *buffer;

  /*
   * Allocate a protection key:
   */
  pkey = __pkey_alloc();
  printf("pkey allocated = %d\n", pkey);
  if (pkey == -1)
    errExit("pkey_alloc");

  /*
   * Disable access to any memory with "pkey" set,
   * even though there is none right now
   */
  status = __pkey_set(pkey, PKEY_DISABLE_ACCESS, 0);
  if (!status)
    errExit("pkey_set");

  simple_set();
  simple_zeroed_set();
  simple_realloc();
  complex_set();

  status = __pkey_free(pkey);
  if (status == -1)
    errExit("pkey_free");
  exit(EXIT_SUCCESS);
}