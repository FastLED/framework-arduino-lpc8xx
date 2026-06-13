// SPDX-License-Identifier: BSD-3-Clause
#include "Wire.h"
#include "Stream.h"

namespace {

// TwoWireImpl — the actual I2C implementation. TU-local (anonymous namespace)
// so the whole class is DCE'd when nothing references it. Inherits `Stream`
// so virtual dispatch over Stream still works internally.
class TwoWireImpl : public Stream {
public:
    void begin(void) {}
    void begin(uint8_t address) { _address = address; }
    void end(void) {}
    void setClock(uint32_t clock) { _clock = clock; }
    void setWireTimeout(uint32_t timeout, bool reset_with_timeout) {
        (void)timeout;
        (void)reset_with_timeout;
        _timeoutFlag = false;
    }
    bool getWireTimeoutFlag(void) const { return _timeoutFlag; }
    void clearWireTimeoutFlag(void) { _timeoutFlag = false; }

    void beginTransmission(uint8_t address) { _address = address; }
    size_t write(uint8_t data) override {
        (void)data;
        return 1u;
    }
    size_t write(const uint8_t *data, size_t quantity) override {
        (void)data;
        return quantity;
    }
    uint8_t endTransmission(void) { return endTransmission(true); }
    uint8_t endTransmission(bool stopBit) {
        (void)stopBit;
        return 0u;
    }
    uint8_t requestFrom(uint8_t address, uint8_t quantity) {
        return requestFrom(address, quantity, true);
    }
    uint8_t requestFrom(uint8_t address, uint8_t quantity, bool stopBit) {
        (void)address;
        (void)quantity;
        (void)stopBit;
        return 0u;
    }

    int available(void) override { return 0; }
    int read(void) override { return -1; }
    int peek(void) override { return -1; }
    void flush(void) override {}

private:
    uint8_t _address = 0u;
    uint32_t _clock = 100000u;
    bool _timeoutFlag = false;
};

// Meyers singleton — first call constructs the TwoWireImpl. If nothing in
// the link references this accessor, the linker drops the whole impl class.
TwoWireImpl& impl(void) {
    static TwoWireImpl instance;
    return instance;
}

}  // namespace

// `Wire` is a trivially-constructible proxy. No ctor body, no vtable, no
// `__cxa_atexit` dtor registration — it occupies 1 byte in `.bss` and pulls
// nothing at static-init time.
TwoWire Wire;

// Proxy methods delegate to the lazily-constructed singleton. If a sketch
// never calls a `Wire` method, none of these delegators are referenced and
// `--gc-sections` drops them + the singleton + TwoWireImpl entirely.
void TwoWire::begin(void) { impl().begin(); }
void TwoWire::begin(uint8_t address) { impl().begin(address); }
void TwoWire::end(void) { impl().end(); }
void TwoWire::setClock(uint32_t clock) { impl().setClock(clock); }
void TwoWire::setWireTimeout(uint32_t timeout, bool reset_with_timeout) {
    impl().setWireTimeout(timeout, reset_with_timeout);
}
bool TwoWire::getWireTimeoutFlag(void) const { return impl().getWireTimeoutFlag(); }
void TwoWire::clearWireTimeoutFlag(void) { impl().clearWireTimeoutFlag(); }

void TwoWire::beginTransmission(uint8_t address) { impl().beginTransmission(address); }
size_t TwoWire::write(uint8_t data) { return impl().write(data); }
size_t TwoWire::write(const uint8_t *data, size_t quantity) {
    return impl().write(data, quantity);
}
uint8_t TwoWire::endTransmission(void) { return impl().endTransmission(); }
uint8_t TwoWire::endTransmission(bool stopBit) { return impl().endTransmission(stopBit); }
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity) {
    return impl().requestFrom(address, quantity);
}
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, bool stopBit) {
    return impl().requestFrom(address, quantity, stopBit);
}

int TwoWire::available(void) { return impl().available(); }
int TwoWire::read(void) { return impl().read(); }
int TwoWire::peek(void) { return impl().peek(); }
void TwoWire::flush(void) { impl().flush(); }

// Conversion to the underlying Stream — returns a reference to the lazily-
// constructed `TwoWireImpl` (which IS-A `Stream`). Only referenced when
// callers actually treat `Wire` as a `Stream`; otherwise --gc-sections drops
// this conversion (and, transitively, the rest of the impl).
TwoWire::operator Stream&() { return impl(); }
