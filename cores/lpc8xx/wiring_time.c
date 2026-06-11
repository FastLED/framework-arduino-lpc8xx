// SPDX-License-Identifier: BSD-3-Clause
#include "Arduino.h"
#include "lpc8xx_registers.h"

#define SYSTICK_CTRL_ENABLE    (1u << 0)
#define SYSTICK_CTRL_TICKINT   (1u << 1)
#define SYSTICK_CTRL_CLKSOURCE (1u << 2)

__attribute__((weak)) uint32_t SystemCoreClock = F_CPU;

static volatile uint32_t g_systick_millis = 0;
static uint8_t g_systick_started = 0;

__attribute__((weak)) void SystemCoreClockUpdate(void) {
    SystemCoreClock = F_CPU;
}

static uint32_t ticks_per_ms(void) {
    return SystemCoreClock / 1000UL;
}

static uint32_t ticks_per_us(void) {
    return SystemCoreClock / 1000000UL;
}

void init_systick(void) {
    if (g_systick_started) {
        return;
    }

    SystemCoreClockUpdate();
    const uint32_t ticks = ticks_per_ms();
    if (ticks == 0u || ticks > 0x1000000UL) {
        return;
    }

    LPC8XX_SYSTICK->LOAD = ticks - 1u;
    LPC8XX_SYSTICK->VAL = 0u;
    LPC8XX_SYSTICK->CTRL =
        SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
    g_systick_started = 1u;
}

void SysTick_Handler(void) {
    ++g_systick_millis;
}

uint32_t millis(void) {
    init_systick();
    return g_systick_millis;
}

uint32_t micros(void) {
    init_systick();

    const uint32_t per_us = ticks_per_us();
    if (!g_systick_started || per_us == 0u) {
        return g_systick_millis * 1000UL;
    }

    uint32_t before;
    uint32_t fraction;
    uint32_t after;
    do {
        before = g_systick_millis;
        const uint32_t period_ticks = LPC8XX_SYSTICK->LOAD + 1u;
        const uint32_t current_ticks = LPC8XX_SYSTICK->VAL;
        const uint32_t elapsed_ticks = period_ticks - current_ticks;
        fraction = elapsed_ticks / per_us;
        if (fraction > 999UL) {
            fraction = 999UL;
        }
        after = g_systick_millis;
    } while (before != after);

    return before * 1000UL + fraction;
}

void delay(uint32_t ms) {
    const uint32_t start = millis();
    while ((millis() - start) < ms) {
        yield();
    }
}

void delayMicroseconds(uint32_t us) {
    const uint32_t start = micros();
    while ((micros() - start) < us) {
    }
}

void yield(void) {}
