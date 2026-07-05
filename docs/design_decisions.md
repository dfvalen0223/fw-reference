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