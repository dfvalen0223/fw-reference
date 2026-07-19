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
- **HIL (Hardware-in-the-Loop) testing** — RS-485 protocol verified on real
  hardware against an RP2040 responder, signal-level validation with a logic analyzer
- **RS-485 protocol stack** (COBS framing + CRC-16 + stop-and-wait ACK/retry),
  same encode/decode source compiled into both ends of the link
- **MCP2515 CAN controller driver** over bit-banged SPI, validated both via
  internal chip loopback and a real two-node physical CAN bus
  (LPC1768 <-> RP2040-Zero, 125 kbps, One-Shot Mode) — signal-level
  proof with a logic analyzer, see [docs/captures/README.md](docs/captures/README.md)
- **Bit-banged SPI HAL** (LPC1768 + RP2040) — replaces the Pico SDK's
  `hardware_spi` peripheral after it was proven to corrupt data on rapid
  back-to-back short transfers
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

## Hardware-in-the-Loop (HIL) Testing

The RS-485 protocol runs end-to-end on real hardware:

```
LPC1768 (96 MHz, FreeRTOS)                RP2040-Zero
  UART2 TX  p28 ────────────────────────► GP5 RX
  UART2 RX  p27 ◄──────────────────────── GP4 TX      115200 8N1
  GND ──────────────────────────────────── GND
```

- `firmware.bin` (LPC1768) — FreeRTOS app: sends a DATA frame every second
  via the full protocol (COBS + CRC-16 + ACK with retries)
- `rp2040/protocol_responder.cpp` (RP2040) — validates CRC, replies ACK,
  returns the payload; compiled from the **same** COBS/CRC sources as the LPC side
- `tests/rs485_test.cpp` — bare-metal loopback self-test (LED1 pass / LED4 fail)
- Timing and frame content verified with a Saleae logic analyzer
  (measured 115385 baud, +0.16% of nominal) — see
  [docs/captures/](docs/captures/README.md) for the annotated capture
  showing the full DATA → ACK → DATA → ACK exchange and the DE
  (driver-enable) half-duplex arbitration

## CAN Bus (MCP2515) HIL Testing

Real two-node CAN bus, no simulated/loopback shortcuts:

```
LPC1768 (bit-bang SPI)              RP2040-Zero (bit-bang SPI)
  MCP2515 #1  ── CANH/CANL ──────────  MCP2515 #2      125 kbps
  (ID 0x200, TJA1050 @ 5V)             (ID 0x100, TJA1050 @ 5V)
```

- `src/drivers/mcp2515.cpp` — MCP2515 driver (register map, bit-timing
  config, send/receive, One-Shot Mode), same source built for both ends
- `tests/mcp2515_bus_test.cpp` (LPC1768) / `rp2040/can_node.cpp`
  (RP2040) — each node sends an incrementing counter and reports
  reception via LED/WS2812
- `rp2040/can_loopback_test.cpp` — standalone MCP2515 internal-loopback
  triage tool (no bus/transceiver dependency)
- Getting the physical bus working required finding and fixing four
  stacked hardware faults (bad transceiver, SPI SI/SO swap, CANH/CANL
  swap, a module connector routing the termination resistor in series
  instead of across the bus) — full writeup and logic-analyzer captures
  in [docs/captures/README.md](docs/captures/README.md)

## Tools

Python scripts for cross-validation, simulation and debugging:

- `tools/crc_validator.py` — CRC-16 CCITT implementation, generates 100 random test vectors
- `tools/bmp280_stub.py` — BMP280 pressure/temperature frame simulator
- `tools/serial_hexterm.py` — HTerm-style serial monitor (hex + ASCII, timestamps, hex TX)

```bash
python3 tools/crc_validator.py  # generates test_vectors.csv
python3 tools/bmp280_stub.py    # prints 10 sample frames
python3 tools/serial_hexterm.py /dev/tty.usbmodem21302 115200
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
