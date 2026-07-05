# Firmware Reference

[![CI](https://github.com/dfvalen0223/fw-reference/actions/workflows/ci.yml/badge.svg)](https://github.com/dfvalen0223/fw-reference/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> Production-grade firmware reference for embedded systems in harsh environments.
> Built on NXP LPC1768 (ARM Cortex-M3). Designed for reliability, testability,
> and field maintainability.

## Highlights

- **Hardware Abstraction Layer (HAL)** with mockable interfaces (UART, GPIO, SPI, Timer)
- **SIL (Software-in-the-Loop) testing** on host — no hardware required to run tests
- **CRC-16 CCITT** with canonical test vector verification (0x29B1)
- **Robust telemetry framing protocol** with guard clauses and error propagation
- **CMake + Ninja** build system with cross-compile support for ARM Cortex-M3
- **Continuous Integration** via GitHub Actions with coverage reporting
- **Static analysis** with cppcheck on every push

## Build & Test

```bash
# Configure (first run downloads GoogleTest via FetchContent)
cmake -B build -G Ninja

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## Architecture
┌─────────────────────────────────┐
│         Application Layer       │  		(main.cpp )
├─────────────────────────────────┤
│           Driver Layer          │  		(TelemetryProtocol, sensors)
│   depends on HAL interfaces 	  │◄─────── not on concrete hardware
├─────────────────────────────────┤
│        HAL Interface Layer      │  		(IUart, IGpio, ISpi, ITimer)
├───────────────┬─────────────────┤
│  HAL LPC1768  │  HAL Mock       │
│  (target)     │  (host tests)   │
└───────────────┴─────────────────┘

## Why This Exists

This project demonstrates senior-level embedded firmware practices: dependency injection via hardware abstraction, SIL testing with mocks, and defensive programming patterns (guard clauses, CRC validation, safe-state recovery) required for systems that cannot be easily serviced in the field.

Built on prior experience with the LPC1768.

## License

MIT — see [LICENSE](LICENSE).