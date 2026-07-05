# Architecture

## Layer Overview

The firmware follows a layered architecture with strict dependency direction:
Application → Drivers → HAL Interface → HAL Implementation → Hardware ↓ HAL Mock (tests only)


## Key Principle: Dependency Inversion

High-level modules (drivers) depend on abstractions (HAL interfaces), not on concrete hardware implementations. This means:

1. **Drivers are testable without hardware** — inject a mock in tests
2. **Hardware can change** — swap LPC1768 for STM32 without touching drivers
3. **Tests run on x86_64 host** — fast iteration, no flash/Debug cycle

## Build Modes

| Mode | Command | Purpose |
|------|---------|---------|
| Host | `cmake -B build -G Ninja` | Unit tests on Mac/Linux (no hardware) |
| Target | `cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-toolchain.cmake` | Cross-compile for LPC1768 (Day 7+) |