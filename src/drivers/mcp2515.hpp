#pragma once

#include "hal/hal_spi.hpp"
#include <cstdint>
#include <cstddef>

namespace drivers {

/**
 * @brief Microchip MCP2515 stand-alone CAN controller driver, SPI mode.
 *
 * Instruction set, register map, and CAN control register semantics per
 * Microchip datasheet DS20001801K (MCP2515 Family Data Sheet):
 *   - Section 12.0 "SPI Interface", Table 12-1 (SPI instruction set, p. 67)
 *   - Section 11.0 "Register Map", Table 11-1/11-2 (pp. 63)
 *   - Section 10.0 "Modes of Operation", Register 10-1 CANCTRL / 10-2
 *     CANSTAT (pp. 59-61)
 *
 * Register addresses used here (cross-checked against Table 11-1's
 * column/row layout and the LOAD TX BUFFER / READ RX BUFFER figures,
 * which independently confirm the same addresses):
 *   CANSTAT=0x0E  CANCTRL=0x0F  CNF3=0x28  CNF2=0x29  CNF1=0x2A
 *   CANINTE=0x2B  CANINTF=0x2C
 *   TXB0CTRL=0x30 TXB0SIDH=0x31 TXB0SIDL=0x32 TXB0DLC=0x35 TXB0D0..D7=0x36-0x3D
 *   RXB0CTRL=0x60 RXB0SIDH=0x61 RXB0SIDL=0x62 RXB0DLC=0x65 RXB0D0..D7=0x66-0x6D
 *
 * This driver only handles standard (11-bit) CAN IDs — extended (29-bit)
 * IDs are out of scope. CNF1/CNF2/CNF3 bit-timing register values are
 * supplied by the caller (computed for the actual oscillator frequency
 * in use); this driver does not compute bit timing itself.
 */
class Mcp2515 {
public:
    enum class Mode : uint8_t {
        NORMAL      = 0b000,
        SLEEP       = 0b001,
        LOOPBACK    = 0b010,
        LISTEN_ONLY = 0b011,
        CONFIG      = 0b100,
    };

    struct Frame {
        uint16_t id = 0;     // standard 11-bit CAN ID (0-0x7FF)
        uint8_t dlc = 0;     // data length, 0-8
        uint8_t data[8] = {};
    };

    /// Blocking microsecond delay, supplied by the caller (this driver
    /// has no notion of CPU clock speed, so it can't busy-wait itself).
    using DelayUsFn = void (*)(uint32_t microseconds);

    explicit Mcp2515(hal::ISpi& spi, uint8_t slave_index = 0)
        : spi_(spi), slave_index_(slave_index) {}

    /// Resets the chip (enters Configuration mode automatically per
    /// sec 10.1), waits out the Oscillator Start-up Timer (128 OSC1
    /// cycles, sec 8.1 — no SPI transaction is valid before this
    /// elapses), writes CNF1/CNF2/CNF3, then switches to `mode`.
    /// `osc_freq_hz` is the MCP2515's own crystal/resonator frequency
    /// (used only to size the OST wait, not related to the CAN bit
    /// rate). Returns false on SPI failure or if `mode` isn't confirmed
    /// in CANSTAT within the retry budget.
    bool init(uint8_t cnf1, uint8_t cnf2, uint8_t cnf3, Mode mode,
              uint32_t osc_freq_hz, DelayUsFn delay_us);

    /// Loads TXB0 and requests transmission (LOAD TX BUFFER + RTS).
    bool send(const Frame& frame);

    /// If RX0IF (CANINTF -CAN Interrupt Flags- bit 0) is set, reads RXB0 into `frame` and
    /// returns true. The read clears RX0IF automatically (sec 12.4).
    bool receive(Frame& frame);

    /// True if CANINTF RX0IF is set (a frame is waiting in RXB0).
    bool has_message();

private:
    hal::ISpi& spi_;
    uint8_t slave_index_;

    static constexpr uint8_t INSTR_RESET      = 0xC0;  // Table 12-1
    static constexpr uint8_t INSTR_READ       = 0x03;
    static constexpr uint8_t INSTR_WRITE      = 0x02;
    static constexpr uint8_t INSTR_RTS_TXB0   = 0x81;  // 1000 0001, nnn=001
    static constexpr uint8_t INSTR_READ_RXB0  = 0x90;  // 1001 0000, n=0,m=0 -> RXB0SIDH
    static constexpr uint8_t INSTR_LOAD_TXB0  = 0x40;  // 0100 0000, a=b=c=0 -> TXB0SIDH

    static constexpr uint8_t REG_CANSTAT = 0x0E;
    static constexpr uint8_t REG_CANCTRL = 0x0F;
    static constexpr uint8_t REG_CNF3    = 0x28;
    static constexpr uint8_t REG_CANINTF = 0x2C;

    bool reset();
    bool write_reg(uint8_t reg, uint8_t value);
    bool read_reg(uint8_t reg, uint8_t& value);
    bool set_mode(Mode mode);
};

}  // namespace drivers
