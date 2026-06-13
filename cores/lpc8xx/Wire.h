// SPDX-License-Identifier: BSD-3-Clause
#ifndef ARDUINO_CORE_LPC8XX_WIRE_H
#define ARDUINO_CORE_LPC8XX_WIRE_H

#include <stddef.h>
#include <stdint.h>

// Forward-declare Stream so the conversion operator below resolves without
// pulling Stream's full definition (and its virtual function table) into
// every TU that includes Wire.h.
class Stream;

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
// Stream-polymorphism: `TwoWire` no longer inherits from `Stream`, but it
// DOES provide an implicit `operator Stream&()` that returns a reference to
// the (lazily-constructed) underlying impl — itself a `Stream`. Existing
// patterns like `Stream& s = Wire; s.write(...);` keep working. The
// conversion operator is dead code if the sketch never uses it, so
// `--gc-sections` still strips the impl in the common case.
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

    // Implicit conversion to the underlying Stream so existing code that
    // expects `Stream& s = Wire;` or passes Wire where a Stream& is required
    // continues to compile. Forwards to the lazily-constructed singleton —
    // if no caller uses this operator, the linker strips it.
    operator Stream&();
};

extern TwoWire Wire;

#endif
