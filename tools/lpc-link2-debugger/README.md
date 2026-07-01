# LPC-Link2 debugger reflash bundle

Assets used by `fbuild deploy` (FastLED/fbuild#921 follow-up) to auto-upgrade
the on-board LPC-Link2 debugger firmware on the LPC845-BRK and LPC804-EVK.
Once the debugger runs the newer CMSIS-DAP-V2 firmware, its CDC serial-bridge
forwards host `DTR` / `RTS` to the target's `!RESET` / `!ISP` pins, which is
what lets `lpc21isp -control` auto-enter ISP mode without the human SW3+SW4
button press.

## Contents

| File | What it is | Origin |
| --- | --- | --- |
| `lpc-link2-cmsis-dap-v2.hex` | LPC-Link2 CMSIS-DAP V2 (WinUSB) firmware — recommended | ARM `CMSIS-DAP` reference build, vendored via Microchip's `dev_packs` mirror of CMSIS 5.8.0 (`arm/CMSIS/5.8.0/CMSIS/DAP/Firmware/Examples/LPC-Link2/V2/Objects/CMSIS_DAP.hex`) |
| `lpc-link2-cmsis-dap-v1.hex` | LPC-Link2 CMSIS-DAP V1 (HID) firmware — legacy fallback | Same as above, `V1/Objects/CMSIS_DAP.hex` |
| `dfu-util-0.11-windows-x86_64.zip` | dfu-util 0.11 + libusb-1.0.dll, Windows x86_64 | Official dfu-util 0.11 binary release (Tormod Volden), https://dfu-util.sourceforge.net/releases/dfu-util-0.11-binaries.tar.xz |
| `dfu-util-0.11-linux-x86_64.tar.gz` | dfu-util 0.11, Linux x86_64 | Same |
| `dfu-util-0.11-darwin-x86_64.tar.gz` | dfu-util 0.11, macOS x86_64 | Same |
| `SHA256SUMS` | Integrity manifest for every file above | Generated locally when the bundle was assembled |

## Reflash flow

1. Put the LPC845-BRK / LPC804-EVK debugger into DFU mode by holding the
   ISP-select jumper / short (LPC845-BRK: solder-short **JP1** to GND) at
   power-up. The board re-enumerates as `1FC9:000C` LPC-Link2 in DFU.
2. Run `dfu-util --alt 0 --download lpc-link2-cmsis-dap-v2.hex --reset`.
3. After reset the debugger runs the new firmware; `-control` auto-ISP works
   on future `fbuild deploy` invocations.

`fbuild` automates step 2 once the tools are cached under
`~/.fbuild/{prod|dev}/tools/lpc-link2-debugger/` — see FastLED/fbuild PR
implementing `fbuild deploy … --upgrade-debugger` for details.

## Licensing

- `lpc-link2-cmsis-dap-v*.hex`: ARM CMSIS-DAP reference firmware, redistributed
  under the Apache-2.0 terms carried by CMSIS 5.8.0's `LICENSE.txt`. See
  https://github.com/ARM-software/CMSIS_5/blob/develop/LICENSE.txt .
- `dfu-util-0.11-*`: GPLv2 (dfu-util) + LGPLv2.1 (libusb-1.0). Bundled
  `COPYING` retained inside each archive.

Both are freeware; redistribution alongside fbuild is fair-use per the FastLED
project maintainer.

## Bumping

Refresh the CMSIS-DAP firmware hex by pointing at the same relative path on a
newer CMSIS release. The current pin is 5.8.0 (the last release before ARM
moved the repo to `ARM-software/CMSIS_5`).

Refresh dfu-util by fetching a newer archive from
https://dfu-util.sourceforge.net/releases/ , unpacking, and re-zipping the
per-platform subdirectory alongside a copy of `COPYING` + `README-bin.txt`.
Regenerate `SHA256SUMS` afterwards.
