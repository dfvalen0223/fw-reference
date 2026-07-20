# Design Decisions

## Why C++ interfaces (not C function pointers)?

C++ virtual interfaces provide type safety, RAII (Resource Acquisition Is Initialization; for example `std::array<uint8_t, 68> frame{}` in telemetry_protocol.cpp:27 — the stack buffer is acquired upon declaration and automatically freed upon exiting scope. There is no `malloc/free`), and integrate naturally with GoogleMock. Function pointers work but are error-prone (no type checking on callbacks, manual lifecycle management).

## Why FetchContent (not system-installed GoogleTest)?

Self-contained repo: clone → build → test works on any machine without preinstalling dependencies. Critical for CI reproducibility.

## Why CRC-16 CCITT (not CRC-32)?

CRC-16 is sufficient for short frames (≤64 bytes payload). CCITT variant is standard in industrial protocols. Smaller checksum = smaller frames = less bandwidth on low-speed RS-485 links.

## Why bitwise CRC (not lookup table)?

Bitwise implementation uses zero extra RAM (important for constrained targets, like MCUs). Lookup table (256 bytes) is faster but unnecessary at 115200 baud (14 kB/s, CRC computes in microseconds).

## Why Ninja (not Make)?

Ninja is 2-3x faster for incremental builds and handles CMake dependencies more efficiently. Already available via Homebrew on macOS.

## Why PLL0 at M=12, N=1 (FCCO = 288 MHz)?

UM10360 (PDF File) requires the PLL0 oscillator (FCCO) to run between **275 and 550 MHz**. With a 12 MHz crystal, `FCCO = 2·M·Fosc/N = 2·12·12/1 = 288 MHz`, then `CCLK = 288/3 = 96 MHz` via CCLKCFG=2. An earlier configuration targeting FCCO = 192 MHz was below the legal range — the PLL never locked and boot hung. Every PLL wait loop now has a timeout with fallback to the 4 MHz internal RC oscillator, so a missing/dead crystal degrades performance instead of bricking boot.

## Why peripheral clocks stay at the reset default (CCLK/4)?

UART divisors assume PCLK = 24 MHz (CCLK/4, the reset default) instead of reprogramming PCLKSEL. Two reasons: (1) 24 MHz gives 115200 baud with a clean integer divisor (DL=13, +0.16% error); (2) PCLKSEL writes were observed being ignored on the test hardware — a divisor strategy that works from reset defaults removes that whole failure class. Verified on the wire with a logic analyzer: 115385 baud measured.

## Why an RP2040 protocol responder (not just an echo)?

A byte-echo can only prove the electrical path. The protocol responder compiles the **same** `util/cobs.cpp` and `util/crc16.hpp` as the LPC1768 firmware and speaks the full frame format (SOF + COBS + CRC-16 + ACK/NACK). This validates framing, CRC, sequence handling and the retry state machine end-to-end on real hardware — a true HIL test, not a loopback illusion.