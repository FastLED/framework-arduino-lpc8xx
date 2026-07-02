// SPDX-License-Identifier: BSD-3-Clause
#include <stdint.h>

#include "Arduino.h"
#include "lpc8xx_registers.h"

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
#if defined(ARDUINOCORE_LPC8XX_NO_FAULT_EMIT) && ARDUINOCORE_LPC8XX_NO_FAULT_EMIT
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
#else
void NMI_Handler(void);
void HardFault_Handler(void);
#endif
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

#if !(defined(ARDUINOCORE_LPC8XX_NO_FAULT_EMIT) && ARDUINOCORE_LPC8XX_NO_FAULT_EMIT)
/* ---------------------------------------------------------------------------
 * Fault diagnostic emitter (issue #30).
 *
 * Replaces the silent Default_Handler infinite-loop for HardFault and NMI with
 * a direct USART0 sync-write of the saved exception frame's PC, LR, xPSR,
 * followed by a software reset via AIRCR.SYSRESETREQ. No printf, no Serial,
 * no allocator -- those paths may themselves be broken if we got here from a
 * heap fault or USART driver fault.
 *
 * Opt out with -DARDUINOCORE_LPC8XX_NO_FAULT_EMIT=1 (build.extra_flags) to
 * restore the original Default_Handler weak-alias (silent loop) behavior.
 *
 * Refs:
 *   - ARMv6-M Architecture Reference Manual B1.5.4 (exception stack frame).
 *   - LPC845 user manual UM11029 ch.13 (USART register map).
 * --------------------------------------------------------------------------- */

static void fault_emit_byte(char c) {
    /* STAT.TXIDLE (bit 3) -- wait for the previous byte to leave the shift
       register before we overwrite TXDAT. Using TXIDLE rather than TXRDY
       guarantees the line is fully drained on the final byte before reset. */
    while (!(LPC8XX_USART0->STAT & (1u << 3))) {
    }
    LPC8XX_USART0->TXDAT = (uint32_t)(uint8_t)c;
}

static void fault_emit_str(const char *s) {
    while (*s) {
        fault_emit_byte(*s++);
    }
}

static void fault_emit_hex32(uint32_t v) {
    for (int i = 28; i >= 0; i -= 4) {
        const uint32_t nyb = (v >> i) & 0xFu;
        fault_emit_byte((char)(nyb < 10u ? ('0' + nyb) : ('A' + nyb - 10u)));
    }
}

void HardFault_Handler(void) {
    /* Read MSP and grab the saved exception frame.
       Layout (ARMv6-M B1.5.4):
         sp[0]=R0  sp[1]=R1  sp[2]=R2   sp[3]=R3
         sp[4]=R12 sp[5]=LR  sp[6]=PC   sp[7]=xPSR */
    uint32_t *msp;
    __asm volatile ("MRS %0, MSP" : "=r"(msp));
    const uint32_t pc  = msp[6];
    const uint32_t lr  = msp[5];
    const uint32_t psr = msp[7];

    fault_emit_str("\r\nFAULT: HardFault PC=0x");
    fault_emit_hex32(pc);
    fault_emit_str(" LR=0x");
    fault_emit_hex32(lr);
    fault_emit_str(" xPSR=0x");
    fault_emit_hex32(psr);
    fault_emit_str("\r\n");

    /* Drain spin -- ~1 ms at 30 MHz to let the LPC11U35 USB-VCOM bridge
       on LPC845-BRK flush its FIFO before the chip resets. */
    for (volatile int i = 0; i < 30000; ++i) {
        __asm volatile ("nop");
    }

    /* AIRCR.SYSRESETREQ -- software reset back to a known boot state. */
    *(volatile uint32_t *)0xE000ED0Cu = (0x05FAu << 16) | (1u << 2);
    for (;;) {
    }
}

/* NMI gets the same emit + reboot path as HardFault. Weak so a sketch or
 * library can install its own NMI handler (e.g. FastLED routes the WWDT
 * warning interrupt to NMI via SYSCON->NMISRC for pre-reset wedge
 * backtraces and may want a WDT-specific report). */
void NMI_Handler(void) __attribute__((weak, alias("HardFault_Handler")));
#endif /* !ARDUINOCORE_LPC8XX_NO_FAULT_EMIT */

/* ---------------------------------------------------------------------------
 * Named weak chip-level IRQ handlers (issue #38).
 *
 * Standard CMSIS startup pattern: every chip-level vector slot gets a
 * peripheral-named handler, weak-aliased to Default_Handler, so a sketch
 * or library can install an ISR by simply defining the strong symbol
 * (e.g. `void DMA0_IRQHandler(void)`). Previously all 32 slots pointed
 * at Default_Handler directly, which made ISR-driven drivers (DMA chunk
 * refill, async UART TX) impossible without a RAM vector table — and the
 * Cortex-M0+ has no VTOR, so that would need SYSMEMREMAP plus a reserved
 * block at the start of SRAM in every linker script.
 *
 * Slot names follow each chip's IRQn enum in its vendor CMSIS header
 * (variants/<chip>/LPC8xx.h). Reserved slots keep Default_Handler.
 * Chips without a per-chip block below keep the legacy all-default
 * table — no behavior change until their map is added.
 * --------------------------------------------------------------------------- */
#if defined(__LPC845__) || defined(__LPC804__)
void SPI0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DAC0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void USART0_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void I2C1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void I2C0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void MRT0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void CMP_CAPT_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void WDT_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void BOD_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void FLASH_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void WKT_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void CTIMER0_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void PIN_INT0_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT1_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT2_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT3_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT4_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
#endif
#if defined(__LPC845__)
void SPI1_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void SCT0_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void ADC0_SEQA_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void ADC0_SEQB_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void ADC0_THCMP_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void ADC0_OVR_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void DMA0_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C2_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C3_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void PIN_INT5_DAC1_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT6_USART3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void PIN_INT7_USART4_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
#elif defined(__LPC804__)
void ADC_SEQA_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void ADC_SEQB_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void ADC_THCMP_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void ADC_OVR_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void PIN_INT5_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT6_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void PIN_INT7_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
#endif

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
    
    // Chip Level - LPC8xx IRQs (32 total). Slot names per the chip's
    // vendor CMSIS IRQn enum; reserved slots stay on Default_Handler.
#if defined(__LPC845__)
    SPI0_IRQHandler,             // IRQ0  SPI0
    SPI1_IRQHandler,             // IRQ1  SPI1
    DAC0_IRQHandler,             // IRQ2  DAC0
    USART0_IRQHandler,           // IRQ3  USART0
    USART1_IRQHandler,           // IRQ4  USART1
    USART2_IRQHandler,           // IRQ5  USART2
    Default_Handler,             // IRQ6  reserved
    I2C1_IRQHandler,             // IRQ7  I2C1
    I2C0_IRQHandler,             // IRQ8  I2C0
    SCT0_IRQHandler,             // IRQ9  SCT0
    MRT0_IRQHandler,             // IRQ10 MRT0
    CMP_CAPT_IRQHandler,         // IRQ11 analog comparator / captouch
    WDT_IRQHandler,              // IRQ12 WWDT
    BOD_IRQHandler,              // IRQ13 BOD
    FLASH_IRQHandler,            // IRQ14 flash
    WKT_IRQHandler,              // IRQ15 self-wake-up timer
    ADC0_SEQA_IRQHandler,        // IRQ16 ADC0 seq A
    ADC0_SEQB_IRQHandler,        // IRQ17 ADC0 seq B
    ADC0_THCMP_IRQHandler,       // IRQ18 ADC0 threshold compare
    ADC0_OVR_IRQHandler,         // IRQ19 ADC0 overrun
    DMA0_IRQHandler,             // IRQ20 DMA0
    I2C2_IRQHandler,             // IRQ21 I2C2
    I2C3_IRQHandler,             // IRQ22 I2C3
    CTIMER0_IRQHandler,          // IRQ23 CTIMER0
    PIN_INT0_IRQHandler,         // IRQ24 pin int 0
    PIN_INT1_IRQHandler,         // IRQ25 pin int 1
    PIN_INT2_IRQHandler,         // IRQ26 pin int 2
    PIN_INT3_IRQHandler,         // IRQ27 pin int 3
    PIN_INT4_IRQHandler,         // IRQ28 pin int 4
    PIN_INT5_DAC1_IRQHandler,    // IRQ29 pin int 5 / DAC1
    PIN_INT6_USART3_IRQHandler,  // IRQ30 pin int 6 / USART3
    PIN_INT7_USART4_IRQHandler,  // IRQ31 pin int 7 / USART4
#elif defined(__LPC804__)
    SPI0_IRQHandler,             // IRQ0  SPI0
    Default_Handler,             // IRQ1  reserved
    DAC0_IRQHandler,             // IRQ2  DAC0
    USART0_IRQHandler,           // IRQ3  USART0
    USART1_IRQHandler,           // IRQ4  USART1
    Default_Handler,             // IRQ5  reserved
    Default_Handler,             // IRQ6  reserved
    I2C1_IRQHandler,             // IRQ7  I2C1
    I2C0_IRQHandler,             // IRQ8  I2C0
    Default_Handler,             // IRQ9  reserved
    MRT0_IRQHandler,             // IRQ10 MRT0
    CMP_CAPT_IRQHandler,         // IRQ11 analog comparator / captouch
    WDT_IRQHandler,              // IRQ12 WWDT
    BOD_IRQHandler,              // IRQ13 BOD
    FLASH_IRQHandler,            // IRQ14 flash
    WKT_IRQHandler,              // IRQ15 self-wake-up timer
    ADC_SEQA_IRQHandler,         // IRQ16 ADC seq A
    ADC_SEQB_IRQHandler,         // IRQ17 ADC seq B
    ADC_THCMP_IRQHandler,        // IRQ18 ADC threshold compare
    ADC_OVR_IRQHandler,          // IRQ19 ADC overrun
    Default_Handler,             // IRQ20 reserved
    Default_Handler,             // IRQ21 reserved
    Default_Handler,             // IRQ22 reserved
    CTIMER0_IRQHandler,          // IRQ23 CTIMER0
    PIN_INT0_IRQHandler,         // IRQ24 pin int 0
    PIN_INT1_IRQHandler,         // IRQ25 pin int 1
    PIN_INT2_IRQHandler,         // IRQ26 pin int 2
    PIN_INT3_IRQHandler,         // IRQ27 pin int 3
    PIN_INT4_IRQHandler,         // IRQ28 pin int 4
    PIN_INT5_IRQHandler,         // IRQ29 pin int 5
    PIN_INT6_IRQHandler,         // IRQ30 pin int 6
    PIN_INT7_IRQHandler,         // IRQ31 pin int 7
#else
    // Chips without a named map yet keep the legacy all-default table.
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
#endif
};

void SystemInit(void) {
#if defined(__LPC845__)
    lpc845_fro_direct_enable();
#endif
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

// newlib's __libc_init_array (called above) ends with a call to _init; the
// matching __libc_fini_array calls _fini. These are normally supplied by
// crti.o/crtn.o, which a -nostartfiles bare-metal link does not pull in,
// leaving an undefined reference. Constructors run purely via the linker's
// .init_array section, so empty weak stubs satisfy the references.
__attribute__((weak)) void _init(void) {}
__attribute__((weak)) void _fini(void) {}
