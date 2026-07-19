#include "mcp2515.hpp"

namespace drivers {

namespace {
constexpr uint32_t SPI_TIMEOUT_MS = 100;
}

bool Mcp2515::reset() {
    uint8_t tx = INSTR_RESET;
    uint8_t rx = 0;
    // ALWAYS this check the spi status before send something
    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    auto status = spi_.transfer(&tx, &rx, 1, SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    return status == hal::ISpi::Status::OK;
}

bool Mcp2515::write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx[3] = {INSTR_WRITE, reg, value};
    uint8_t rx[3] = {};
    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    auto status = spi_.transfer(tx, rx, sizeof(tx), SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    return status == hal::ISpi::Status::OK;
}

bool Mcp2515::read_reg(uint8_t reg, uint8_t& value) {
    uint8_t tx[3] = {INSTR_READ, reg, 0x00}; // 0x00 arbitrary value, don't care
    uint8_t rx[3] = {};
    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    auto status = spi_.transfer(tx, rx, sizeof(tx), SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    if (status != hal::ISpi::Status::OK) return false;
    value = rx[2];
    return true;
}

bool Mcp2515::set_mode(Mode mode) {
    // CANCTRL bits 7:5 = REQOP[2:0] (Register 10-1, p. 60). Read-Modify-Write (RMW)
    // to preserve the other bits (ABAT, OSM, CLKEN, CLKPRE).
    uint8_t canctrl = 0;
    // READ
    if (!read_reg(REG_CANCTRL, canctrl)) return false;
    // MODIFY: keep bits[4:0] (ABAT, OSM, CLKEN, CLKPRE), discard the old
    // REQOP bits[7:5] and OR in the new mode.
    canctrl = static_cast<uint8_t>((canctrl & 0x1F) | (static_cast<uint8_t>(mode) << 5));
    // WRITE
    if (!write_reg(REG_CANCTRL, canctrl)) return false;

    // Poll CANSTAT OPMOD[2:0] (bits 7:5, Register 10-2, p. 61) until it
    // reflects the requested mode.
    // VERIFY 1000 TIMES THE CORRECT VALUES WERE WRITTEN ON THE REGS
    for (int attempt = 0; attempt < 1000; attempt++) {
        uint8_t canstat = 0;
        if (!read_reg(REG_CANSTAT, canstat)) return false;
        if (((canstat >> 5) & 0x07) == static_cast<uint8_t>(mode)) return true;
    }
    return false;
}

bool Mcp2515::init(uint8_t cnf1, uint8_t cnf2, uint8_t cnf3, Mode mode,
                   uint32_t osc_freq_hz, DelayUsFn delay_us) {
    // RESET (sec 10.1) automatically selects Configuration mode, the
    // only mode where CNF1/CNF2/CNF3 are writable.
    if (!reset()) return false;

    // Oscillator Start-up Timer (sec 8.1): the device holds itself in
    // Reset for 128 OSC1 clock cycles after RESET (SPI or pin) before
    // its internal state machine starts. "No SPI protocol operations
    // should be attempted until after the OST has expired." Ceiling
    // division so the wait is never short by a fraction of a cycle.
    if (osc_freq_hz == 0) return false;
    const uint32_t ost_us =
        static_cast<uint32_t>((128ULL * 1000000ULL + osc_freq_hz - 1) / osc_freq_hz);
    delay_us(ost_us);

    if (!write_reg(REG_CNF3, cnf3)) return false;
    if (!write_reg(REG_CNF3 + 1, cnf2)) return false;  // CNF2 = 0x29
    if (!write_reg(REG_CNF3 + 2, cnf1)) return false;  // CNF1 = 0x2A

    return set_mode(mode);
}

bool Mcp2515::send(const Frame& frame) {
    if (frame.id > 0x7FF || frame.dlc > 8) return false;

    // Standard frame header (Register 3-3 TXBnSIDH / 3-4 TXBnSIDL, p. 20):
    //   SIDH[7:0] = SID[10:3]
    //   SIDL[7:5] = SID[2:0]; SIDL bit 3 = EXIDE, 0 selects a standard
    //   (11-bit) identifier for this TX buffer — leaving it 0 here is
    //   what makes this a standard, not extended, frame.
    const uint8_t sidh = static_cast<uint8_t>(frame.id >> 3);
    const uint8_t sidl = static_cast<uint8_t>((frame.id & 0x07) << 5);

    // LOAD TX BUFFER (Figure 12-5, p. 69): 0x40 points the Address
    // Pointer at TXB0SIDH; subsequent bytes clock in sequentially, same
    // as a WRITE, covering SIDH,SIDL,EID8,EID0,DLC,D0..D7. (SPI Reg Map Order REGISTER 3-3)
    // SIDH → SIDL → EID8 → EID0 → DLC(+RTR)
    uint8_t tx[1 + 5 + 8] = {};
    tx[0] = INSTR_LOAD_TXB0;
    tx[1] = sidh;
    tx[2] = sidl;
    tx[3] = 0x00;  // EID8 (unused, standard frame)
    tx[4] = 0x00;  // EID0 (unused, standard frame)
    tx[5] = frame.dlc & 0x0F;
    for (uint8_t i = 0; i < frame.dlc; i++) tx[6 + i] = frame.data[i];

    const std::size_t len = 6 + frame.dlc;
    uint8_t rx[1 + 5 + 8] = {};

    // Because SPI is Full Duplex, we send an receive @ same time.
    // We don't care about what we received this time.
    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    auto status = spi_.transfer(tx, rx, len, SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    if (status != hal::ISpi::Status::OK) return false;

    // RTS (Figure 12-6, p. 69): single-byte instruction, sets TXREQ for
    // TXB0 (nnn bit0), begins the transmission sequence.
    uint8_t rts_tx = INSTR_RTS_TXB0;
    uint8_t rts_rx = 0;
    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    status = spi_.transfer(&rts_tx, &rts_rx, 1, SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    return status == hal::ISpi::Status::OK;
}

bool Mcp2515::has_message() {
    uint8_t canintf = 0;
    if (!read_reg(REG_CANINTF, canintf)) return false;
    return (canintf & 0x01) != 0;  // RX0IF = CANINTF bit 0 (Table 11-2)
}

bool Mcp2515::receive(Frame& frame) {
    // READ RX BUFFER (Figure 12-3, p. 68): 0x90 points the Address
    // Pointer at RXB0SIDH; sequential clocks read SIDH,SIDL,EID8,EID0,
    // DLC,D0..D7. Per sec 12.4, RX0IF is auto-cleared when CS rises.
    uint8_t tx[1 + 5 + 8] = {INSTR_READ_RXB0};
    uint8_t rx[1 + 5 + 8] = {};

    if (spi_.select_slave(slave_index_) != hal::ISpi::Status::OK) return false;
    auto status = spi_.transfer(tx, rx, sizeof(tx), SPI_TIMEOUT_MS);
    spi_.deselect_slave(slave_index_);
    if (status != hal::ISpi::Status::OK) return false;

    const uint8_t sidh = rx[1];
    const uint8_t sidl = rx[2];
    // DLC is a 4-bit field (0-15) but Frame::data is only 8 bytes (CAN
    // payload is never >8 bytes); a corrupt/unexpected read here would
    // otherwise write past the end of frame.data.
    uint8_t dlc = rx[5] & 0x0F;
    if (dlc > 8) dlc = 8;

    frame.id = static_cast<uint16_t>((sidh << 3) | (sidl >> 5));
    frame.dlc = dlc;
    for (uint8_t i = 0; i < dlc; i++) frame.data[i] = rx[6 + i];
    for (uint8_t i = dlc; i < 8; i++) frame.data[i] = 0;

    return true;
}

}  // namespace drivers
