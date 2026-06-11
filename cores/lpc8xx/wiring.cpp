// SPDX-License-Identifier: BSD-3-Clause
// Arduino wiring initialization for LPC8xx

#include "Arduino.h"
#include "lpc8xx_registers.h"

extern "C" {

// Arduino init() function - called by main() before setup()
// This sets up basic system clocks and peripherals
void init(void) {
    // The LPC845 comes out of reset running from FRO (Free Running Oscillator) at 12 MHz
    // For now, we'll just use that default clock
    // TODO: Add option to configure PLL for higher speeds (30 MHz max)
    
    // Enable clocks for basic peripherals needed by Arduino
    lpc8xx_basic_peripheral_init();  // GPIO, IOCON, SWM (already defined in variant.cpp's initVariant)
    
    init_systick();

    // Note: Timer/UART/SPI/ADC clocks are enabled on-demand by their respective classes
}

// Stub yield() if not defined elsewhere
void __attribute__((weak)) yield(void) {
    // Can add WFI (wait for interrupt) here if desired
}

}
