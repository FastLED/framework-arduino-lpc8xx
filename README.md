# `tools` branch — patched probe-rs source + cross-compile CI

**This is an orphan branch** on `FastLED/framework-arduino-lpc8xx` — completely separate history from `main` (which carries the Arduino LPC8xx framework itself). This branch's sole purpose is to hold the source tree of the FastLED-patched fork of [`probe-rs`](https://github.com/probe-rs/probe-rs) plus the GitHub Action that cross-compiles it for every platform fbuild needs.

## Why an orphan branch

- The framework repo's `main` shouldn't carry ~150 MB of unrelated Rust source.
- A separate FastLED-org repo for the probe-rs fork was more repo sprawl than the actual scope justifies.
- An orphan branch here keeps *all LPC-Link2 debugger tooling* (dfu-util + firmware hexes on `main`'s `tools/lpc-link2-debugger/`, probe-rs on this branch's Rust tree) under one repository roof.

## Patches applied (relative to upstream `probe-rs/probe-rs`)

1. **DAP_Info sub-command fallbacks** — the LPC-Link2 v1.0.7 firmware silently drops DAP_Info queries for packet size / packet count / capabilities. probe-rs used to bail with `NoPacketSize`; now it logs `warn!` and falls back to CMSIS-DAP 2.1.0 spec defaults (64-byte HID reports, single-packet transfer, SWD-only capability set).
2. **DAP_Connect explicit-SWD + retry** — `DefaultPort` is optional per spec and the v1.0.7 firmware doesn't implement it. Using explicit `Swd` (0x01) plus a 4× retry rides out the "first HID report after enumeration gets dropped" behavior on Windows.

Both patches are strict improvements against **any** conforming probe with a slow first Connect or a firmware missing optional info fields.

## What the GitHub Action does

`.github/workflows/fastled-release-cross.yml` cross-compiles `probe-rs` for six targets:

- `x86_64-pc-windows-msvc`
- `aarch64-pc-windows-msvc`
- `x86_64-unknown-linux-gnu`
- `aarch64-unknown-linux-gnu`
- `x86_64-apple-darwin`
- `aarch64-apple-darwin`

Triggers:

- **`workflow_dispatch`** — manual "rebuild the current tip" run.
- **Push of a tag matching `fastled-v*`** — cuts a GitHub Release with all six archives + a `SHA256SUMS` manifest attached.

Each archive contains the `probe-rs` binary, upstream `LICENSE-*` files, and a `FASTLED-BUILD.txt` naming the exact commit + which FastLED patches were applied. fbuild fetches whichever archive matches the host triple at first-run `fbuild deploy … --upgrade-debugger` (or the future "prefer probe-rs when a CMSIS-DAP probe enumerates" dispatch path).

Related: FastLED/fbuild#921, FastLED/fbuild#935.
