// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#if defined(CPU_LPC845M301JBD48) || defined(__LPC845__) || defined(LPC845)
#ifndef CPU_LPC845M301JBD48
#define CPU_LPC845M301JBD48 1
#endif
#include "LPC845.h"
#elif defined(CPU_LPC804M101JDH24) || defined(__LPC804__) || defined(LPC804)
#ifndef CPU_LPC804M101JDH24
#define CPU_LPC804M101JDH24 1
#endif
#include "LPC804.h"
#else
#error "No supported LPC8xx part selected"
#endif

// Backward-compat: pull in the Arduino-flavored register shims after the
// vendor CMSIS PAL. lpc8xx_registers.h provides LPC8XX_SYSCON / LPC8XX_SPI0 /
// LPC8XX_IOCON etc. that user sketches and earlier core revisions depended
// on. Names do not collide with the vendor's SCT0/DMA0/SYSCON/SPI0/etc.
// pointer macros, so both sets remain available.
#include "lpc8xx_registers.h"
