// SPDX-License-Identifier: BSD-3-Clause
#ifndef ARDUINO_CORE_LPC8XX_WIRE_H
#define ARDUINO_CORE_LPC8XX_WIRE_H

#include <stddef.h>
#include <stdint.h>

#define BUFFER_LENGTH 32

// TwoWire — proxy facade for the I2C / Wire library.
//
// `class TwoWire` is intentionally a trivially-constructible empty proxy: no
// members, no virtual functions, no `Stream` inheritance. Methods forward
// to a function-local-static implementation defined in Wire.cpp.
//
// Why: the previous `class TwoWire : public Stream { ... }` with a global
// `TwoWire Wire;` instance pulled the full TwoWire vtable + ctor/dtor + every
// method into every link, even when the sketch did not call any Wire method.
// `--gc-sections` could not strip it because the global instance's vtable +
// dtor (registered via `__cxa_atexit`) anchored everything. On the 64 KB-
// FLASH LPC845, that alone could tip the link over budget.
//
// Trade-off: `TwoWire` is no longer a `Stream` subclass — `Stream& s = Wire;`
// no longer compiles. Direct `Wire.begin()` / `Wire.write()` calls work as
// before. Sketches that need polymorphic `Stream*` access to Wire can
// instantiate a local `TwoWire` (or build the impl directly).
class TwoWire {
public:
    void begin(void);
    void begin(uint8_t address);
    void end(void);
    void setClock(uint32_t clock);
    void setWireTimeout(uint32_t timeout = 25000, bool reset_with_timeout = false);
    bool getWireTimeoutFlag(void) const;
    void clearWireTimeoutFlag(void);

    void beginTransmission(uint8_t address);
    size_t write(uint8_t data);
    size_t write(const uint8_t *data, size_t quantity);
    uint8_t endTransmission(void);
    uint8_t endTransmission(bool stopBit);
    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool stopBit);

    int available(void);
    int read(void);
    int peek(void);
    void flush(void);
};

extern TwoWire Wire;

#endif
