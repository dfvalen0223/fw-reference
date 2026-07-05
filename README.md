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
- **Mock SPI and Timer** for comprehensive SIL testing
- **CRC-16 cross-validator** in Python (100 random test vectors)
- **BMP280 sensor stub** for pressure/temperature simulation

## Build & Test

```bash
# Configure (first run downloads GoogleTest via FetchContent)
cmake -B build -G Ninja

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Local coverage (macOS, requires llvm-cov)
cmake -B build -G Ninja -DENABLE_COVERAGE=ON
cmake --build build
ninja -C build coverage
```

## Tools

Python scripts for cross-validation and simulation:

- `tools/crc_validator.py` — CRC-16 CCITT implementation, generates 100 random test vectors
- `tools/bmp280_stub.py` — BMP280 pressure/temperature frame simulator

```bash
python3 tools/crc_validator.py  # generates test_vectors.csv
python3 tools/bmp280_stub.py    # prints 10 sample frames
```

## Architecture

```
┌─────────────────────────────────┐
│         Application Layer       │   (main.cpp)
├─────────────────────────────────┤
│           Driver Layer          │   (TelemetryProtocol, sensors)
│   depends on HAL interfaces     │◄────── not on concrete hardware
├─────────────────────────────────┤
│        HAL Interface Layer      │   (IUart, IGpio, ISpi, ITimer)
├───────────────┬─────────────────┤
│  HAL LPC1768  │  HAL Mock       │
│  (target)     │  (host tests)   │
└───────────────┴─────────────────┘
```

## Why This Exists

This project shows senior-level embedded firmware practices — dependency injection through hardware abstraction, SIL testing with mocks, and defensive patterns (guard clauses, CRC validation, safe-state recovery) needed for systems that can't be easily serviced in the field.

Built on prior work with the LPC1768 in safety-critical medical firmware.

## License

MIT — see [LICENSE](LICENSE).
