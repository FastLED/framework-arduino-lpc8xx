// SPDX-License-Identifier: BSD-3-Clause
#include "SPI.h"
#include "lpc8xx_registers.h"

namespace {

static const uint8_t SWM_SPI0_SCK = 15u;
static const uint8_t SWM_SPI0_MOSI = 16u;
static const uint8_t SWM_SPI0_MISO = 17u;
static const uint8_t SWM_SPI0_SSEL0 = 18u;

uint8_t divider_from_clock(uint32_t clock) {
    if (clock == 0u) {
        return SPI_CLOCK_DIV4;
    }
    uint32_t divider = F_CPU / clock;
    if (divider < 2u) {
        divider = 2u;
    }
    if (divider > 255u) {
        divider = 255u;
    }
    return (uint8_t)divider;
}

// SPIClassImpl — the actual SPI implementation. TU-local so the whole class
// gets DCE'd when nothing references it.
class SPIClassImpl {
public:
    void begin(void) {
        lpc8xx_basic_peripheral_init();
        lpc8xx_enable_clock(LPC8XX_AHBCLK_SPI0);
        lpc8xx_reset_peripheral(LPC8XX_PRESET_SPI0);

        lpc8xx_swm_assign(SWM_SPI0_SCK,
                          lpc8xx_swm_pin(g_APinDescription[PIN_SPI_SCK].port,
                                         g_APinDescription[PIN_SPI_SCK].bit));
        lpc8xx_swm_assign(SWM_SPI0_MOSI,
                          lpc8xx_swm_pin(g_APinDescription[PIN_SPI_MOSI].port,
                                         g_APinDescription[PIN_SPI_MOSI].bit));
        lpc8xx_swm_assign(SWM_SPI0_MISO,
                          lpc8xx_swm_pin(g_APinDescription[PIN_SPI_MISO].port,
                                         g_APinDescription[PIN_SPI_MISO].bit));
        lpc8xx_swm_assign(SWM_SPI0_SSEL0,
                          lpc8xx_swm_pin(g_APinDescription[PIN_SPI_SS].port,
                                         g_APinDescription[PIN_SPI_SS].bit));

        _begun = true;
        applyConfig();
    }

    void end(void) {
        LPC8XX_SPI0->CFG = 0u;
        _begun = false;
    }

    void beginTransaction(SPISettings settings) {
        if (!_begun) {
            begin();
        }
        applySettings(settings);
    }

    void endTransaction(void) {}

    void usingInterrupt(uint8_t interruptNumber) { (void)interruptNumber; }
    void notUsingInterrupt(uint8_t interruptNumber) { (void)interruptNumber; }

    uint8_t transfer(uint8_t b) {
        while ((LPC8XX_SPI0->STAT & LPC8XX_SPI_STAT_TXRDY) == 0u) {
        }
        LPC8XX_SPI0->TXDAT = b;
        while ((LPC8XX_SPI0->STAT & LPC8XX_SPI_STAT_RXRDY) == 0u) {
        }
        return (uint8_t)(LPC8XX_SPI0->RXDAT & 0xFFu);
    }

    uint16_t transfer16(uint16_t data) {
        uint16_t result;
        if (_bitOrder == LSBFIRST) {
            result = transfer((uint8_t)(data & 0xFFu));
            result |= (uint16_t)transfer((uint8_t)(data >> 8)) << 8;
        } else {
            result = (uint16_t)transfer((uint8_t)(data >> 8)) << 8;
            result |= transfer((uint8_t)(data & 0xFFu));
        }
        return result;
    }

    void transfer(void *buf, size_t len) {
        uint8_t *p = static_cast<uint8_t *>(buf);
        for (size_t i = 0; i < len; ++i) {
            p[i] = transfer(p[i]);
        }
    }

    void setClockDivider(uint8_t divider) {
        _divider = divider == 0u ? SPI_CLOCK_DIV4 : divider;
        if (_begun) {
            applyConfig();
        }
    }

    void setBitOrder(uint8_t order) {
        _bitOrder = order;
        if (_begun) {
            applyConfig();
        }
    }

    void setDataMode(uint8_t mode) {
        _dataMode = mode & 0x3u;
        if (_begun) {
            applyConfig();
        }
    }

private:
    void applyConfig(void) {
        uint32_t cfg = LPC8XX_SPI_CFG_MASTER;
        if (_bitOrder == LSBFIRST) {
            cfg |= (1u << 3);
        }
        if ((_dataMode & 0x1u) != 0u) {
            cfg |= (1u << 4);
        }
        if ((_dataMode & 0x2u) != 0u) {
            cfg |= (1u << 5);
        }

        LPC8XX_SPI0->CFG = cfg;
        LPC8XX_SPI0->DIV = _divider > 0u ? (uint32_t)(_divider - 1u) : 0u;
        LPC8XX_SPI0->DLY = 0u;
        LPC8XX_SPI0->CFG = cfg | LPC8XX_SPI_CFG_ENABLE;
    }

    void applySettings(SPISettings settings) {
        _divider = divider_from_clock(settings.clock());
        _bitOrder = settings.bitOrder();
        _dataMode = settings.dataMode() & 0x3u;
        applyConfig();
    }

    uint8_t _divider = SPI_CLOCK_DIV4;
    uint8_t _bitOrder = MSBFIRST;
    uint8_t _dataMode = SPI_MODE0;
    bool _begun = false;
};

// Meyers singleton — first call constructs the SPIClassImpl. If nothing
// references this accessor, the linker drops the whole impl class.
SPIClassImpl& impl(void) {
    static SPIClassImpl instance;
    return instance;
}

}  // namespace

// `SPI` is a trivially-constructible proxy. No ctor body, no vtable, no
// `__cxa_atexit` dtor registration. See SPI.h for the rationale.
SPIClass SPI;

void SPIClass::begin(void) { impl().begin(); }
void SPIClass::end(void) { impl().end(); }
void SPIClass::beginTransaction(SPISettings settings) { impl().beginTransaction(settings); }
void SPIClass::endTransaction(void) { impl().endTransaction(); }
void SPIClass::usingInterrupt(uint8_t interruptNumber) { impl().usingInterrupt(interruptNumber); }
void SPIClass::notUsingInterrupt(uint8_t interruptNumber) {
    impl().notUsingInterrupt(interruptNumber);
}
uint8_t SPIClass::transfer(uint8_t b) { return impl().transfer(b); }
uint16_t SPIClass::transfer16(uint16_t data) { return impl().transfer16(data); }
void SPIClass::transfer(void *buf, size_t len) { impl().transfer(buf, len); }
void SPIClass::setClockDivider(uint8_t divider) { impl().setClockDivider(divider); }
void SPIClass::setBitOrder(uint8_t order) { impl().setBitOrder(order); }
void SPIClass::setDataMode(uint8_t mode) { impl().setDataMode(mode); }
