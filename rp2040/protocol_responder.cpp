// RS-485 protocol responder for HIL testing, on Waveshare RP2040-Zero.
//
// Speaks the drivers::Rs485Protocol wire format:
//   [SOF=0x55][COBS( [TYPE][SEQ][PAYLOAD...][CRC16-BE] )][0x00]
//
// On a valid DATA frame: replies ACK(seq), then sends the payload back
// in its own DATA frame (same seq). On CRC error: replies NACK(seq).
// Incoming ACK/NACK frames (from the LPC acking our DATA) are ignored.
//
// Shares the exact COBS + CRC16 implementation with the LPC firmware.
//
// WS2812 status (GP16): 3 blue = boot, green = DATA ok, red = CRC error.

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "util/cobs.hpp"
#include "util/crc16.hpp"

#include <cstring>

#define UART_ID    uart1
#define BAUD_RATE  115200
#define TX_PIN     4
#define RX_PIN     5
#define DE_PIN     6

#define WS2812_PIN 16

static constexpr uint8_t SOF       = 0x55;
static constexpr uint8_t TYPE_DATA = 0x01;
static constexpr uint8_t TYPE_ACK  = 0x02;
static constexpr uint8_t TYPE_NACK = 0x03;

static inline void put_pixel(uint32_t grb) {
    pio_sm_put_blocking(pio0, 0, grb << 8u);
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

static void send_wire(const uint8_t* raw, std::size_t raw_len) {
    uint8_t wire[300];
    wire[0] = SOF;
    std::size_t n = util::cobs_encode(raw, raw_len, wire + 1, sizeof(wire) - 1);
    if (n == 0) return;

    gpio_put(DE_PIN, 1);
    uart_write_blocking(UART_ID, wire, 1 + n);
    uart_tx_wait_blocking(UART_ID);
    gpio_put(DE_PIN, 0);
}

static void send_ctrl(uint8_t type, uint8_t seq) {
    uint8_t f[4] = {type, seq, 0, 0};
    uint16_t crc = util::crc16_ccitt(f, 2);
    f[2] = (uint8_t)(crc >> 8);
    f[3] = (uint8_t)(crc & 0xFF);
    send_wire(f, sizeof(f));
}

static void send_data(uint8_t seq, const uint8_t* payload, std::size_t len) {
    uint8_t f[2 + 128 + 2];
    f[0] = TYPE_DATA;
    f[1] = seq;
    std::memcpy(&f[2], payload, len);
    uint16_t crc = util::crc16_ccitt(f, 2 + len);
    f[2 + len]     = (uint8_t)(crc >> 8);
    f[2 + len + 1] = (uint8_t)(crc & 0xFF);
    send_wire(f, 2 + len + 2);
}

int main(void) {
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, WS2812_PIN, 800000, false);

    for (int i = 0; i < 3; i++) {
        put_pixel(rgb(0, 0, 32)); sleep_ms(150);
        put_pixel(0);             sleep_ms(150);
    }

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RX_PIN, GPIO_FUNC_UART);

    gpio_init(DE_PIN);
    gpio_set_dir(DE_PIN, GPIO_OUT);
    gpio_put(DE_PIN, 0);

    uint8_t cobs_buf[300];
    std::size_t idx = 0;
    bool in_frame = false;

    while (true) {
        if (!uart_is_readable(UART_ID)) continue;
        uint8_t byte = uart_getc(UART_ID);

        if (!in_frame) {
            if (byte == SOF) { in_frame = true; idx = 0; }
            continue;
        }

        if (idx >= sizeof(cobs_buf)) { in_frame = false; continue; }
        cobs_buf[idx++] = byte;
        if (byte != 0x00) continue;

        // Full COBS frame collected
        in_frame = false;

        uint8_t raw[300];
        std::size_t n = util::cobs_decode(cobs_buf, idx, raw, sizeof(raw));
        if (n < 4) continue;

        uint16_t rx_crc = ((uint16_t)raw[n - 2] << 8) | raw[n - 1];
        uint16_t calc   = util::crc16_ccitt(raw, n - 2);
        uint8_t  type   = raw[0];
        uint8_t  seq    = raw[1];

        if (rx_crc != calc) {
            put_pixel(rgb(32, 0, 0));   // red: corrupt
            send_ctrl(TYPE_NACK, seq);
            sleep_ms(30);
            put_pixel(0);
            continue;
        }

        if (type == TYPE_DATA) {
            put_pixel(rgb(0, 32, 0));   // green: valid DATA
            send_ctrl(TYPE_ACK, seq);
            send_data(seq, &raw[2], n - 4);   // payload back to the LPC
            sleep_ms(30);
            put_pixel(0);
        }
        // ACK/NACK from the LPC: ignore
    }
}
