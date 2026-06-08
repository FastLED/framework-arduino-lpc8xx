# ArduinoCore-LPC8xx

Arduino-compatible support for NXP LPC804 and LPC845 Cortex-M0+ boards.

This repository is structured as a standalone Arduino 1.5+ hardware package.
It can be installed manually for Arduino IDE or Arduino CLI, and it is intended
to be the package that fbuild consumes rather than a separate fbuild-only
scaffold.

> [!WARNING]
> **Alpha status.** This repository currently compiles and links, but deployment
> to LPC804/LPC845 hardware has not been tested end to end. Help is wanted to
> lock in the deployment story and verify correct upload/debug behavior across
> the supported boards and tools. AI-generated pull requests and issues are
> welcome.

## Supported FQBNs

| FQBN | Board | MCU | Board model | Status |
|---|---|---|---|---|
| `zackees:lpc8xx:lpc845brk` | LPC845-BRK | LPC845 | Breakout/prototyping | Compile package available |
| `zackees:lpc8xx:lpcxpresso804` | LPCXpresso804 | LPC804 | 3.3 V Arduino-header board | Compile package available |
| `zackees:lpc8xx:lpcxpresso845max` | LPCXpresso845-MAX | LPC845 | 3.3 V Arduino-header board | Provisional pin map |

LPC845-BRK is not an Arduino UNO shield target. LPCXpresso804 and
LPCXpresso845-MAX expose Arduino-style headers, but they are 3.3 V evaluation
boards, not 5 V UNO clones.

## Manual Arduino CLI Install

Clone or link this repository into the sketchbook hardware directory:

```powershell
mkdir "$HOME\Documents\Arduino\hardware\zackees"
cmd /c mklink /J "%USERPROFILE%\Documents\Arduino\hardware\zackees\lpc8xx" "%CD%"
arduino-cli board listall lpc8xx
arduino-cli compile --fqbn zackees:lpc8xx:lpc845brk examples\Blink
```

On macOS/Linux, use:

```sh
mkdir -p "$HOME/Arduino/hardware/zackees"
ln -s "$PWD" "$HOME/Arduino/hardware/zackees/lpc8xx"
arduino-cli board listall lpc8xx
arduino-cli compile --fqbn zackees:lpc8xx:lpc845brk examples/Blink
```

The package uses Arduino's installed `arm-none-eabi-gcc` tool. If the tool is
not present yet, install an Arduino ARM core such as `arduino:sam` first.

## Package Contents

| File | Purpose |
|---|---|
| `boards.txt` | Board IDs, FQBNs, memory sizes, variants, upload defaults |
| `platform.txt` | GCC compile/link recipes, binary/hex output, OpenOCD upload recipe |
| `programmers.txt` | CMSIS-DAP/OpenOCD default and manual ISP fallback |
| `linker_scripts/gcc/*.ld` | LPC804/LPC845 flash and RAM layouts |
| `cores/lpc8xx/startup_lpc8xx.c` | Reset handler, data/BSS init, constructors, CRP word |
| `tools/lpc8xx_image_check.py` | Vector checksum and CRP validator |

The LPC vector checksum word at offset `0x1c` is emitted by the linker script.
The CRP word is placed at `0x2fc` with the safe erased value `0xffffffff`.

## Implemented API Surface

| Area | Current support |
|---|---|
| Entry point | `main()` calls `init()`, `initVariant()`, `setup()`, and `loop()` |
| GPIO | `pinMode`, `digitalWrite`, `digitalRead` |
| Timing | SysTick-backed `millis`, `micros`, `delay`, `delayMicroseconds` |
| Serial | Blocking USART0 `Serial`, `Stream`/`Print` compatible surface |
| SPI | Blocking SPI0 plus `SPISettings`, transactions, `transfer16`, buffer transfer |
| Wire/I2C | Arduino-compatible compile surface; hardware transfer work remains |
| Analog/PWM | Compile surface and resolution APIs; hardware ADC/PWM work remains |
| Interrupts | `digitalPinToInterrupt`, `attachInterrupt`, `detachInterrupt` compile surface |
| Tone | Non-blocking API surface; hardware timer allocation work remains |
| USB | Not exposed. LPC804/LPC845 boards use probe VCOM, not target USB CDC |

## MCU Capability Matrix

| Capability | LPC804 | LPC845/LPC84x |
|---|---:|---:|
| Flash / SRAM | 32 KB / 4 KB | Up to 64 KB / 16 KB |
| I2C | Up to 2 | Up to 4 |
| USART | Up to 2 | Up to 5 |
| SPI | 1 | Up to 2 |
| ADC | 12-bit | 12-bit |
| DAC | 10-bit | Up to 2 DACs |
| PWM/timers | MRT, one CTimer/PWM | SCTimer/PWM, CTimer, MRT |
| DMA | Limited by part | Present |
| Pin interrupts | Shared PININT channels | Shared PININT channels |

## Validation

Representative examples live under `examples/`:

- Blink
- BlinkWithoutDelay
- Button
- AnalogReadSerial
- Fading
- ToneMelody
- Serial
- SPILoopback
- I2CScanner
- FastLEDClocklessSmoke
- FastLEDSpiSmoke

FastLED examples are guarded by `LPC8XX_ENABLE_FASTLED_EXAMPLE` so package
compile checks do not require the FastLED library unless that macro is enabled.

Validate a generated binary with:

```sh
python tools/lpc8xx_image_check.py path/to/sketch.bin
```

Hardware validation for LPC845-BRK and LPCXpresso804 is still required before
the provisional pin maps should be considered final.

## Board Manager Path

Board Manager installation needs a release archive URL, archive size, SHA-256
checksum, and tool dependency metadata. Until a release archive exists, use the
manual hardware-package install above. The package metadata in this repo is
laid out so a `package_zackees_lpc8xx_index.json` can be generated from the
same files when releases are published.

## License

BSD-3-Clause. See [LICENSE](LICENSE).
