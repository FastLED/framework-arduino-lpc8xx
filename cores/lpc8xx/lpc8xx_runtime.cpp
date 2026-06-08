// SPDX-License-Identifier: BSD-3-Clause
// Minimal freestanding C++ runtime support for bare-metal builds.
#include <stddef.h>
#include <stdint.h>

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
char end __attribute__((weak));
char _end __attribute__((weak));

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

void operator delete(void *ptr) noexcept {
    (void)ptr;
}

void operator delete(void *ptr, unsigned int size) noexcept {
    (void)ptr;
    (void)size;
}

void operator delete(void *ptr, unsigned long size) noexcept {
    (void)ptr;
    (void)size;
}

void operator delete[](void *ptr) noexcept {
    (void)ptr;
}

void operator delete[](void *ptr, unsigned int size) noexcept {
    (void)ptr;
    (void)size;
}

void operator delete[](void *ptr, unsigned long size) noexcept {
    (void)ptr;
    (void)size;
}
