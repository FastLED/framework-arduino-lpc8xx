// SPDX-License-Identifier: BSD-3-Clause
// Minimal freestanding C++ runtime support for bare-metal builds.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  // malloc / free

#include "lpc8xx_registers.h"

#define LPC8XX_SCB_AIRCR_ADDR       (0xE000ED0CUL)
#define LPC8XX_AIRCR_VECTKEY        (0x5FAUL << 16)
#define LPC8XX_AIRCR_PRIGROUP_MASK  (7UL << 8)
#define LPC8XX_AIRCR_SYSRESETREQ    (1UL << 2)
#define LPC8XX_ABORT_PAUSE_MS       (20000UL)

static inline volatile uint32_t *lpc8xx_scb_aircr(void) {
    return (volatile uint32_t *)(uintptr_t)LPC8XX_SCB_AIRCR_ADDR;
}

static void lpc8xx_abort_pause(void) {
    // Avoid Arduino delay(): abort can run before SysTick or with interrupts disabled.
    const uint32_t loops_per_ms = (F_CPU / 4000UL) > 0UL ? (F_CPU / 4000UL) : 1UL;
    for (uint32_t ms = 0; ms < LPC8XX_ABORT_PAUSE_MS; ++ms) {
        for (volatile uint32_t spin = loops_per_ms; spin > 0UL; --spin) {
        }
    }
}

static void lpc8xx_system_reset(void) {
    const uint32_t prigroup = *lpc8xx_scb_aircr() & LPC8XX_AIRCR_PRIGROUP_MASK;
    *lpc8xx_scb_aircr() = LPC8XX_AIRCR_VECTKEY | prigroup | LPC8XX_AIRCR_SYSRESETREQ;
}

extern "C" {

void *__dso_handle __attribute__((weak)) = 0;

// `end` / `_end` are NOT defined here. They are the heap-base symbols newlib's
// `_sbrk` references; the linker script (`linker_scripts/gcc/lpc8xx_common.ld`)
// defines them with a forced assignment at the top of `.bss`. Declaring them
// as weak object-file symbols here would PROVIDE-suppress the script's
// definition (a weak def in any object counts as "defined" for `PROVIDE`),
// causing `end` to resolve to an arbitrary address inside `.bss` and the heap
// to grow over live `.bss` data on the first `malloc`. See the script comment.

__attribute__((noreturn)) void abort(void) {
    lpc8xx_abort_pause();
    lpc8xx_system_reset();
    for (;;) {
    }
}

__attribute__((noreturn)) void __cxa_pure_virtual(void) {
    abort();
}

}

// --- operator new / operator new[] -------------------------------------------
// `-nostartfiles` bare-metal links do not pull libstdc++_nano, so the standard
// `operator new` / `operator new[]` are unresolved. Provide thin malloc-backed
// implementations (matching the existing `operator delete` set below). The
// parameter type matches `size_t`, which on this 32-bit ABI is `unsigned int`.
// On a freestanding embedded target we don't throw on allocation failure —
// callers that need detection should invoke `malloc` directly.
void *operator new(unsigned int size) {
    return malloc(size);
}

void *operator new[](unsigned int size) {
    return malloc(size);
}

// All variants route through `free(ptr)`. The sized overloads exist so the
// language ABI is satisfied; on a freestanding nosys target there is no
// sized-aware free, so the size hint is discarded. Inlining keeps the code
// footprint to a single `free` thunk shared by every C++ deallocation. (Note:
// on this 32-bit ABI `unsigned int == unsigned long` widthwise but the
// language treats them as distinct overloads.)
inline void lpc8xx_free(void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr) noexcept { lpc8xx_free(ptr); }
void operator delete(void *ptr, unsigned int) noexcept { lpc8xx_free(ptr); }
void operator delete(void *ptr, unsigned long) noexcept { lpc8xx_free(ptr); }
void operator delete[](void *ptr) noexcept { lpc8xx_free(ptr); }
void operator delete[](void *ptr, unsigned int) noexcept { lpc8xx_free(ptr); }
void operator delete[](void *ptr, unsigned long) noexcept { lpc8xx_free(ptr); }
