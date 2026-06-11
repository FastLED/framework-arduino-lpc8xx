// SPDX-License-Identifier: BSD-3-Clause
#include <stdint.h>

#include "Arduino.h"

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern uint32_t SystemCoreClock;
extern int main(void);
extern void __libc_init_array(void);

__attribute__((section(".crp"), used))
const uint32_t lpc8xx_crp_word = 0xFFFFFFFFu;

void Default_Handler(void) {
    for (;;) {
    }
}

// Forward declarations for exception handlers
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

// External symbols from linker script
extern void _vStackTop(void);
extern void __valid_user_code_checksum(void) __attribute__((weak));

// Provide a default weak implementation for the checksum
void __valid_user_code_checksum_default(void) {}
void __valid_user_code_checksum(void) __attribute__((weak, alias("__valid_user_code_checksum_default")));

// Vector table
extern void (* const g_pfnVectors[])(void);
extern void * __Vectors __attribute__((alias("g_pfnVectors")));

__attribute__((used, section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    // Core Level - Cortex-M0+
    (void (*)(void))(&_vStackTop),     // Initial stack pointer
    Reset_Handler,                      // Reset handler
    NMI_Handler,                        // NMI handler
    HardFault_Handler,                  // HardFault handler
    0,                                  // Reserved
    0,                                  // Reserved
    0,                                  // Reserved
    __valid_user_code_checksum,         // LPC checksum
    0,                                  // Reserved
    0,                                  // Reserved
    0,                                  // Reserved
    SVC_Handler,                        // SVCall handler
    0,                                  // Reserved
    0,                                  // Reserved
    PendSV_Handler,                     // PendSV handler
    SysTick_Handler,                    // SysTick handler
    
    // Chip Level - LPC8xx IRQs (32 total)
    Default_Handler,  // IRQ0
    Default_Handler,  // IRQ1
    Default_Handler,  // IRQ2
    Default_Handler,  // IRQ3
    Default_Handler,  // IRQ4
    Default_Handler,  // IRQ5
    Default_Handler,  // IRQ6
    Default_Handler,  // IRQ7
    Default_Handler,  // IRQ8
    Default_Handler,  // IRQ9
    Default_Handler,  // IRQ10
    Default_Handler,  // IRQ11
    Default_Handler,  // IRQ12
    Default_Handler,  // IRQ13
    Default_Handler,  // IRQ14
    Default_Handler,  // IRQ15
    Default_Handler,  // IRQ16
    Default_Handler,  // IRQ17
    Default_Handler,  // IRQ18
    Default_Handler,  // IRQ19
    Default_Handler,  // IRQ20
    Default_Handler,  // IRQ21
    Default_Handler,  // IRQ22
    Default_Handler,  // IRQ23
    Default_Handler,  // IRQ24
    Default_Handler,  // IRQ25
    Default_Handler,  // IRQ26
    Default_Handler,  // IRQ27
    Default_Handler,  // IRQ28
    Default_Handler,  // IRQ29
    Default_Handler,  // IRQ30
    Default_Handler,  // IRQ31
};

void SystemInit(void) {
    SystemCoreClock = F_CPU;
}

void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;) {
        *dst++ = *src++;
    }

    for (uint32_t *dst = &_sbss; dst < &_ebss;) {
        *dst++ = 0u;
    }

    SystemInit();
    __libc_init_array();
    (void)main();

    for (;;) {
    }
}
