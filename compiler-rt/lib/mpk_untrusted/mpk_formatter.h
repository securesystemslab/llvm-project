#ifndef MPK_FORMATTER_H
#define MPK_FORMATTER_H

#include "alloc_site_handler.h"
#include "mpk_common.h"

namespace __mpk_untrusted {

void flush_allocs();

} // namespace __mpk_untrusted

extern "C" {
// Registers flush_allocs to be called at program exit.
__attribute__((visibility("default"))) static void __attribute__((constructor))
register_flush_allocs();
}

#endif