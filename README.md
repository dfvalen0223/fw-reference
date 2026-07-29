# Firmware Reference

[![CI](https://github.com/dfvalen0223/fw-reference/actions/workflows/ci.yml/badge.svg)](https://github.com/dfvalen0223/fw-reference/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> Production-oriented firmware reference for embedded systems in harsh environments.
> Built on the NXP LPC1768 (ARM Cortex-M3). Designed for reliability, testability,
> and field maintainability.

A scoped, time-boxed demonstration of senior-level embedded practices: dependency
injection through a mockable HAL, software- and hardware-in-the-loop testing, and
defensive patterns for systems that can't be easily serviced in the field. The
protocol stacks below were verified on **real two-node hardware** with a logic
analyzer — not simulated.

## Highlights

- **RS-485 and CAN stacks proven on real hardware** — two independent physical
  links (LPC1768 ↔ RP2040-Zero), signal-level verified with a Saleae logic analyzer.
- **Mockable HAL** (`IUart`, `IGpio`, `ISpi`, `ITimer`) enabling full SIL testing
  on the host with no target hardware required.
- **RS-485 protocol stack** — COBS framing + CRC-16 CCITT + stop-and-wait ACK/retry,
  with the *same* encode/decode source compiled into both ends of the link.
- **MCP2515 CAN driver** over bit-banged SPI, validated on a real CANH/CANL bus
  (125 kbps, One-Shot Mode) after root-causing four stacked hardware faults.
- **CI on every push** — GitHub Actions running the GoogleTest suite, coverage
  reporting, and `cppcheck` static analysis.
- **CMake + Ninja** build with ARM Cortex-M3 cross-compile support.

## System Overview

```
                 ┌───────────────────────────┐
                 │   Logic Analyzer (Saleae) │
                 │ signal-level verification │
                 └────┬───────────┬──────────┘
                      │           │
┌─────────────────────▼───┐   ┌───▼─────────────────────┐
│   LPC1768 (96 MHz)      │   │   RP2040-Zero #1        │
│   FreeRTOS              │   │   RS-485 protocol       │
│                         │◄──┤   responder             │
│  UART2 ── RS-485 ───────┘   │   (COBS+CRC16+ACK)      │
│                         │   └─────────────────────────┘
│  Port2 (bit-bang SPI) ──┐
│  ── MCP2515 #1 (CAN)    │   ┌───────────────────────────┐
│     ID 0x200            │   │   RP2040-Zero #2          │
│                         │   │   CAN node                │
│ P2.4/P2.5 (bit-bang I2C)│   │   MCP2515 #2 (ID 0x100)   │
│ ── BMP280 (planned)     │◄──┤   bit-bang SPI            │
└─────────────────────────┘   └───────────────────────────┘
     CANH/CANL ── real physical 125 kbps bus, both ends
```

Protocols exercised on real hardware (not simulated): **UART/RS-485**
(COBS + CRC-16 + ACK/retry), **CAN** (MCP2515, 125 kbps, One-Shot Mode),
**SPI** (bit-banged, both MCU families), **I2C** (bit-banged, LPC1768).

## Test Results

| Layer            | What                                                                                  | Result |
| ---------------- | ------------------------------------------------------------------------------------- | ------ |
| Host (SIL)       | GoogleTest — CRC-16, COBS, telemetry framing, RS-485 protocol, MCP2515 (mocked SPI)   | **45/45 passing** |
| Cross-validation | Python CRC-16 CCITT vs the C++ implementation, 100 random vectors                      | **100/100 match** |
| HIL — RS-485     | LPC1768 ↔ RP2040-Zero, full DATA→ACK→DATA→ACK round, logic-analyzer verified           | 115385 baud measured (+0.16% of nominal), correct COBS/CRC framing and DE half-duplex arbitration |
| HIL — CAN        | LPC1768 ↔ RP2040-Zero over a real CANH/CANL bus, 125 kbps                              | Bidirectional reception confirmed at the SPI/register level, verified across independent 15 s captures — see [docs/captures/README.md](docs/captures/README.md) |

## Build & Test

```bash
# Configure (first run downloads GoogleTest via FetchContent)
cmake -B build -G Ninja

# Build
cmake --build build

# Run the host test suite
ctest --test-dir build --output-on-failure       # 45/45

# Cross-validate the CRC-16 implementation
python3 tools/crc_validator.py                    # 100/100 vs test_crc16.cpp

# Local coverage (macOS, requires llvm-cov)
cmake -B build -G Ninja -DENABLE_COVERAGE=ON
cmake --build build
ninja -C build coverage
```

## Hardware-in-the-Loop (HIL) Testing

### RS-485

The RS-485 protocol runs end-to-end on real hardware:

```
LPC1768 (96 MHz, FreeRTOS)                RP2040-Zero
  UART2 TX  p28 ────────────────────────► GP5 RX
  UART2 RX  p27 ◄──────────────────────── GP4 TX      115200 8N1
  GND ──────────────────────────────────── GND
```

- `firmware.bin` (LPC1768) — FreeRTOS app sending a DATA frame every second
  through the full protocol (COBS + CRC-16 + ACK with retries).
- `rp2040/protocol_responder.cpp` (RP2040) — validates the CRC, replies ACK, and
  returns the payload; compiled from the **same** COBS/CRC sources as the LPC side.
- `tests/rs485_test.cpp` — bare-metal loopback self-test (LED1 pass / LED4 fail).
- Timing and frame content verified with a Saleae logic analyzer (measured
  115385 baud, +0.16% of nominal) — see [docs/captures/](docs/captures/README.md)
  for the annotated capture of the full DATA → ACK → DATA → ACK exchange and the
  DE (driver-enable) half-duplex arbitration.

### CAN Bus (MCP2515)

A real two-node CAN bus, with no simulated or loopback shortcuts:

```
LPC1768 (bit-bang SPI)              RP2040-Zero (bit-bang SPI)
  MCP2515 #1  ── CANH/CANL ──────────  MCP2515 #2      125 kbps
  (ID 0x200, TJA1050 @ 5V)             (ID 0x100, TJA1050 @ 5V)
```

- `src/drivers/mcp2515.cpp` — MCP2515 driver (register map, bit-timing config,
  send/receive, One-Shot Mode), built for both ends from the same source.
- `tests/mcp2515_bus_test.cpp` (LPC1768) / `rp2040/can_node.cpp` (RP2040) — each
  node sends an incrementing counter and reports reception via LED/WS2812.
- `rp2040/can_loopback_test.cpp` — standalone internal-loopback triage tool with
  no bus or transceiver dependency.

Bringing the physical bus up required finding and fixing **four stacked hardware
faults**: a bad transceiver, a swapped SPI SI/SO pair, swapped CANH/CANL, and a
module connector that routed the termination resistor in series with the bus
instead of across it. Full writeup and logic-analyzer captures in
[docs/captures/README.md](docs/captures/README.md).

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

Drivers depend on HAL *interfaces*, not concrete hardware — so the same driver
code runs against mocks on the host and against silicon on the target.

## Tools

Python scripts for cross-validation, simulation, and debugging:

- `tools/crc_validator.py` — CRC-16 CCITT implementation; generates 100 random
  test vectors (`test_vectors.csv`).
- `tools/bmp280_stub.py` — BMP280 pressure/temperature frame simulator.
- `tools/serial_hexterm.py` — HTerm-style serial monitor (hex + ASCII,
  timestamps, hex TX).

```bash
python3 tools/crc_validator.py                              # generates test_vectors.csv
python3 tools/bmp280_stub.py                                # prints 10 sample frames
python3 tools/serial_hexterm.py /dev/tty.usbmodem21302 115200
```

## Roadmap

Built to prioritize depth (real HIL hardware, full failure-mode debugging,
signal-level proof) over breadth.

**Implemented**

- HAL with mocks + SIL testing, CRC-16, coverage
- LPC1768 bring-up (toolchain, blinky, UART) verified with a logic analyzer
- RS-485 protocol (COBS + CRC + retransmission) over a real two-node link
- CAN driver (MCP2515) over a real two-node physical bus, 125 kbps, including
  root-cause debugging of four stacked hardware faults
- Bit-banged I2C HAL (LPC1768) with a hardware probe test targeting the BMP280

**Planned (out of scope for this pass, in priority order)**

- I2C sensor drivers (BMP280, ICM-20948) — HAL in place, drivers pending
- SD card logging + telemetry data logger (SPI)
- End-to-end telemetry-over-RS-485 integration
- Power management (buck converter, TP4056, 18650) + brown-out simulation
- Automated Python HIL test suite (PC ↔ RP2040 ↔ LPC1768)

## Why This Exists

This project demonstrates senior-level embedded firmware practice — dependency
injection through a hardware abstraction layer, SIL testing with mocks, HIL
verification on real silicon, and defensive patterns (guard clauses, CRC
validation, safe-state recovery) required for systems that can't be easily
serviced in the field. It builds on prior work with the LPC1768 in
safety-critical industrial firmware.

## License

MIT — see [LICENSE](LICENSE).
