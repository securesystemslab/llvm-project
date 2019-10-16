#include "mpk_fault_handler.h"
#include "alloc_site_handler.h"
#include "mpk.h"

#include <sys/mman.h>

namespace __mpk_untrusted {

// Set to whatever the default page size will be for page based MPK.
#define PAGE_SIZE 4096

// Trap Flag
#define TF 0x100

void disableMPK(siginfo_t *si, void *arg);

// General MPK segfault handler. Regardless of MPK access approach, all faults
// will first pass through this handler. The timing of adding this fault handler
// also requires caution for Rust as Rust registers its own fault handler for
// bounds checking that erases all other fault handlers. Thus currently we do
// not instantiate the fault handler in the constructors, but rather on first
// call to the AllocSiteHandler (which occurs the first time any of the
// allocation hooks are called, and thus represent the first time handling of
// MPK faults would be required).
void segMPKHandle(int sig, siginfo_t *si, void *arg) {
  if (si->si_code != SEGV_PKUERR) {
    REPORT("INFO : SegFault other than SEGV_PKUERR, handling with "
           "default handler.\n");
    // SignalHandler was invoked from an error other than MPK violation.
    // Perform default action instead and return.
    if (!prevAction) {
      REPORT("ERROR : prevAction is null, no previous handler to fall back to.\n");
      signal(sig, SIG_DFL);
      raise(sig);
      return;
    }
    if (prevAction->sa_flags & SA_SIGINFO) {
      prevAction->sa_sigaction(sig, si, arg);
    } else if (prevAction->sa_handler == SIG_DFL ||
               prevAction->sa_handler == SIG_IGN) {
      sigaction(sig, prevAction, nullptr);
    } else {
      prevAction->sa_handler(sig);
    }
    return;
  }
  REPORT("INFO : Handling SEGV_PKUERR.\n");

  // Obtains pointer causing fault
  void *ptr = si->si_addr;

  // Obtains the faulting pkey (in SEGV_PKUERR faults)
  uint32_t pkey = si->si_pkey;

  // Get Alloc Site information from the handler.
  auto handler = AllocSiteHandler::getOrInit();
  handler->addFaultAlloc((rust_ptr)ptr, pkey);
  auto fault_site = handler->getAllocSite((rust_ptr)ptr);
  if (!fault_site.isValid()) {
    SINGLE_REPORT("ERROR : Error AllocSite on address: %p; is_safe_addr: %s\n",
                        ptr, is_safe_address(ptr) ? "true" : "false");
  }
  REPORT("INFO : Got Allocation Site (%d) for address: %p with pkey: %d.\n",
         handler->getAllocSite((rust_ptr)ptr).id(), ptr, pkey);
  disableMPK(si, arg);
}

// Disables MPK protection for the given page for the remainder of the runtime.
void disablePageMPK(siginfo_t *si, void *arg) {
  void *page_addr = (void *)((uintptr_t)si->si_addr & ~(PAGE_SIZE - 1));

  REPORT("INFO : Disabling MPK protection for page(%p).\n", page_addr);

  pkey_mprotect(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, 0);
}

// Temporarily disables the given pkey for the current thread.
void disableThreadMPK(void *arg, uint32_t pkey) {
  uint32_t *pkru_ptr = __mpk_untrusted::pkru_ptr(arg);

  auto handler = AllocSiteHandler::getOrInit();
  auto pkey_info = PendingPKeyInfo(pkey, pkey_get(pkru_ptr, pkey));
  handler->storePendingPKeyInfo(gettid(), pkey_info);
  pkey_set(pkru_ptr, pkey, PKEY_ENABLE_ACCESS);

  REPORT("INFO : Pkey(%d) has been set to ENABLE_ACCESS to enable "
         "instruction access.\n",
         pkey);
}

// Re-enables the PendingPKey for the current thread.
void enableThreadMPK(void *arg, PendingPKeyInfo pkey_info) {
  uint32_t *pkru_ptr = __mpk_untrusted::pkru_ptr(arg);
  pkey_set(pkru_ptr, pkey_info.pkey, pkey_info.access_rights);
  REPORT("INFO : Pkey(%d) has been reset to %d.\n", pkey_info.pkey,
         pkey_info.access_rights);
}

void disableMPK(siginfo_t *si, void *arg) {
#if PAGE_MPK
  disablePageMPK(si, arg);
#else
#if SINGLE_STEP
  disableThreadMPK(arg, si->si_pkey);

  // Set trap flag on next instruction
  ucontext_t *uctxt = (ucontext_t *)arg;
  uctxt->uc_mcontext.gregs[REG_EFL] |= TF;
#else
  // TODO : emulateMPK();
#endif
#endif
}

// In the single step approach, we trap after stepping a single instruction and
// then re-enable the pkey in the current thread.
void stepMPKHandle(int sig, siginfo_t *si, void *arg) {
  REPORT("INFO : Reached signal handler after single instruction step.\n");
  auto handler = AllocSiteHandler::getOrInit();
  auto pid_key_info = handler->getAndRemove(getpid());
  if (pid_key_info)
    enableThreadMPK(arg, pid_key_info.getValue());

  // Disable trap flag on next instruction
  ucontext_t *uctxt = (ucontext_t *)arg;
  uctxt->uc_mcontext.gregs[REG_EFL] &= ~TF;
}
} // namespace __mpk_untrusted
