#ifndef MPKSEGFAULTHANDLER_H
#define MPKSEGFAULTHANDLER_H

#include "mpk_common.h"

#include <sys/types.h>
#include <unistd.h>

/// In versions of GLIBC below 2.30, gettid() is not defined and thus needs to
/// be defined directly as a syscall here in the form of a macro.
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))
#endif

namespace __mpk_untrusted {

extern void segMPKHandle(int sig, siginfo_t *si, void *arg);
extern void stepMPKHandle(int sig, siginfo_t *si, void *arg);

} // namespace __mpk_untrusted
#endif
