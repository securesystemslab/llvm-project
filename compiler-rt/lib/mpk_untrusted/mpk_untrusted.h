#ifndef MPKUNTRUSTED_H
#define MPKUNTRUSTED_H

extern struct sigaction *prevAction;

extern "C" {
__attribute__((visibility("default"))) void mpk_SEGV_fault_handler(void *oldAct = nullptr);
__attribute__((visibility("default"))) void mpk_untrusted_constructor();
}

#endif
