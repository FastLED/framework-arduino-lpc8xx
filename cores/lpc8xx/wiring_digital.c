// SPDX-License-Identifier: BSD-3-Clause
// The vendor PAL goes first: SCT_Type has members named INPUT and OUTPUT,
// which Arduino.h defines as macros.
#include "fsl_device_registers.h"  // vendor PAL: IOCON, IOCON_INDEX_PIO0_n
#include "Arduino.h"
#include "lpc8xx_registers.h"

#define IOCON_MODE_MASK     (0x3u << 3)
#define IOCON_MODE_INACT    (0x0u << 3)
#define IOCON_MODE_PULLDOWN (0x1u << 3)
#define IOCON_MODE_PULLUP   (0x2u << 3)

#define LPC8XX_NO_IOCON 0xFFu

/* The IOCON->PIO[] array is not in GPIO-bit order (LPC845/LPC804:
   IOCON_INDEX_PIO0_0 == 17, IOCON_INDEX_PIO0_17 == 0), so map PIO0_n to its
   IOCON slot through the vendor macros. A pad the part lacks has no macro
   (LPC804: PIO0_6, PIO0_31) and gets LPC8XX_NO_IOCON. */
#ifdef IOCON_INDEX_PIO0_6
#define LPC8XX_IOCON_PIO0_6 IOCON_INDEX_PIO0_6
#else
#define LPC8XX_IOCON_PIO0_6 LPC8XX_NO_IOCON
#endif
#ifdef IOCON_INDEX_PIO0_31
#define LPC8XX_IOCON_PIO0_31 IOCON_INDEX_PIO0_31
#else
#define LPC8XX_IOCON_PIO0_31 LPC8XX_NO_IOCON
#endif

static const uint8_t lpc8xx_iocon_index_port0[32] = {
    IOCON_INDEX_PIO0_0,  IOCON_INDEX_PIO0_1,  IOCON_INDEX_PIO0_2,  IOCON_INDEX_PIO0_3,
    IOCON_INDEX_PIO0_4,  IOCON_INDEX_PIO0_5,  LPC8XX_IOCON_PIO0_6, IOCON_INDEX_PIO0_7,
    IOCON_INDEX_PIO0_8,  IOCON_INDEX_PIO0_9,  IOCON_INDEX_PIO0_10, IOCON_INDEX_PIO0_11,
    IOCON_INDEX_PIO0_12, IOCON_INDEX_PIO0_13, IOCON_INDEX_PIO0_14, IOCON_INDEX_PIO0_15,
    IOCON_INDEX_PIO0_16, IOCON_INDEX_PIO0_17, IOCON_INDEX_PIO0_18, IOCON_INDEX_PIO0_19,
    IOCON_INDEX_PIO0_20, IOCON_INDEX_PIO0_21, IOCON_INDEX_PIO0_22, IOCON_INDEX_PIO0_23,
    IOCON_INDEX_PIO0_24, IOCON_INDEX_PIO0_25, IOCON_INDEX_PIO0_26, IOCON_INDEX_PIO0_27,
    IOCON_INDEX_PIO0_28, IOCON_INDEX_PIO0_29, IOCON_INDEX_PIO0_30, LPC8XX_IOCON_PIO0_31,
};

/* LPC845 PIO0_10/PIO0_11 are true open-drain I2C pads: no pull resistors, and
   IOCON bits 5:0 are reserved there (UM11029 Tables 207/208). Never write
   MODE on them. LPC804's PIO0_10/11 are normal pads. */
static int lpc8xx_pin_has_pulls(uint8_t port, uint8_t bit) {
#if defined(__LPC845__)
    return !(port == 0u && (bit == 10u || bit == 11u));
#else
    (void)port;
    (void)bit;
    return 1;
#endif
}

static volatile uint32_t *lpc8xx_iocon_reg(uint8_t port, uint8_t bit) {
    if (port != 0u || bit >= 32u) {
        return 0;
    }
    const uint8_t index = lpc8xx_iocon_index_port0[bit];
    if (index >= sizeof(IOCON->PIO) / sizeof(IOCON->PIO[0])) {  /* incl. NO_IOCON */
        return 0;
    }
    return &IOCON->PIO[index];
}

static int lookup_pin(pin_size_t pin, lpc8xx_pin_info_t *out) {
    if (pin >= variant_pin_count) {
        return 0;
    }
    *out = g_APinDescription[pin];
    return out->bit < 32u;
}

void pinMode(pin_size_t pin, PinMode mode) {
    lpc8xx_pin_info_t info;
    if (!lookup_pin(pin, &info)) {
        return;
    }

    lpc8xx_basic_peripheral_init();

    volatile uint32_t *iocon = lpc8xx_iocon_reg(info.port, info.bit);
    if (iocon != 0 && lpc8xx_pin_has_pulls(info.port, info.bit)) {
        uint32_t value = *iocon & ~IOCON_MODE_MASK;
        if (mode == INPUT_PULLUP) {
            value |= IOCON_MODE_PULLUP;
        } else if (mode == INPUT_PULLDOWN) {
            value |= IOCON_MODE_PULLDOWN;
        } else {
            value |= IOCON_MODE_INACT;
        }
        *iocon = value;
    }

    const uint32_t mask = 1u << info.bit;
    volatile uint32_t *dir = lpc8xx_gpio_dir(info.port);
    if (mode == OUTPUT) {
        *dir |= mask;
    } else {
        *dir &= ~mask;
    }
}

void digitalWrite(pin_size_t pin, PinStatus value) {
    lpc8xx_pin_info_t info;
    if (!lookup_pin(pin, &info)) {
        return;
    }

    const uint32_t mask = 1u << info.bit;
    if (value == LOW) {
        *lpc8xx_gpio_clr(info.port) = mask;
    } else {
        *lpc8xx_gpio_set(info.port) = mask;
    }
}

PinStatus digitalRead(pin_size_t pin) {
    lpc8xx_pin_info_t info;
    if (!lookup_pin(pin, &info)) {
        return LOW;
    }

    const uint32_t mask = 1u << info.bit;
    return ((*lpc8xx_gpio_pin(info.port) & mask) != 0u) ? HIGH : LOW;
}
