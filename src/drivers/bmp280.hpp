#pragma once

#include "hal/hal_spi.hpp"
#include <cstdint>
#include <cstddef>

namespace drivers {

/**
 * @brief Bosch BMP280 digital pressure/temperature sensor driver, SPI mode.
 *
 * Register map, SPI protocol, and compensation formulas per Bosch
 * datasheet BST-BMP280-DS001-26 (Oct 2021):
 *   - Section 4: Global memory map and register description (pp. 24-27)
 *   - Section 5.3: SPI interface (pp. 29-30)
 *   - Section 3.11.3: Compensation formula (pp. 21-22)
 *
 * SPI framing (sec 5.3, p. 30): only 7 bits of the register address are
 * used; bit 7 of the control byte is the R/W bit (0=write, 1=read). The
 * chip supports mode '00' and '11' (CPOL=CPHA); this driver assumes the
 * HAL's ISpi is already configured for one of those.
 */
class Bmp280 {
public:
    static constexpr uint8_t CHIP_ID = 0x58;  // sec 4.3.1, p. 24

    struct Reading {
        int32_t temperature_centidegC;  // 0.01 degC resolution (e.g. 2508 = 25.08 degC)
        uint32_t pressure_pa;           // Pascal
    };

    explicit Bmp280(hal::ISpi& spi, uint8_t slave_index = 0)
        : spi_(spi), slave_index_(slave_index) {}

    /// Reads chip ID, reads calibration trim data, configures oversampling
    /// (x1 temp, x16 pressure) and normal mode. Returns false if the chip
    /// ID doesn't match or any SPI transfer fails.
    bool init();

    /// Burst-reads press+temp registers and returns compensated values.
    /// Returns false on SPI failure. Requires a prior successful init().
    bool read(Reading& out);

private:
    hal::ISpi& spi_;
    uint8_t slave_index_;
    bool initialized_ = false;

    // Calibration trim parameters, sec 4.3.1 / Table 17, p. 21.
    uint16_t dig_T1_ = 0;
    int16_t dig_T2_ = 0, dig_T3_ = 0;
    uint16_t dig_P1_ = 0;
    int16_t dig_P2_ = 0, dig_P3_ = 0, dig_P4_ = 0, dig_P5_ = 0,
             dig_P6_ = 0, dig_P7_ = 0, dig_P8_ = 0, dig_P9_ = 0;

    int32_t t_fine_ = 0;

    bool read_reg(uint8_t reg, uint8_t* buf, std::size_t len);
    bool write_reg(uint8_t reg, uint8_t value);

    // Compensation formulas transcribed verbatim from sec 3.11.3, p. 22
    // (bmp280_compensate_T_int32 / bmp280_compensate_P_int64).
    int32_t compensate_temperature(int32_t adc_T);
    uint32_t compensate_pressure(int32_t adc_P);
};

}  // namespace drivers
