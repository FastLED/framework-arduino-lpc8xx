# cores/lpc8xx

Shared Arduino API surface for the LPC8xx family. Per-MCU details such as pin
maps and default UART/SPI pins live under `variants/<part>/`.

| File | Role |
|---|---|
| `Arduino.h` | Umbrella header; pulls in the selected variant, `HardwareSerial`, and `SPI`. |
| `fsl_device_registers.h` | Minimal NXP-style device-header selector for `LPC804.h` / `LPC845.h`. |
| `lpc8xx_registers.h` | Small direct-register layer used by the core implementation. |
| `wiring_digital.c` | `pinMode`, `digitalWrite`, `digitalRead`. |
| `wiring_time.c` | SysTick-backed `delay`, `delayMicroseconds`, `millis`, `micros`. |
| `HardwareSerial.{h,cpp}` | Blocking USART0 `Serial` implementation. |
| `SPI.{h,cpp}` | Blocking SPI0 implementation. |
| `main.cpp` | Framework-owned `main()` -> `init`, `initVariant`, `setup`, `loop`. |
| `lpc8xx_runtime.cpp` | Minimal freestanding C++ runtime symbols and abort/pure-virtual reset handling. |

The implementation is intentionally small. It is enough for FastLED QA builds
and first board bring-up without importing the full MCUXpresso SDK into the
Arduino core.
