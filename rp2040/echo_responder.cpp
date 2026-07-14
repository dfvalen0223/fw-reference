// RS-485 echo responder for HIL testing, on Waveshare RP2040-Zero.
//
// UART1 on GP4 (TX) / GP5 (RX), DE on GP6 for an optional RS-485
// transceiver. Echoes every received byte.
//
// Status via the on-board WS2812 RGB LED (GP16):
//   3 blue blinks at boot  = firmware alive
//   green flash            = byte received + echoed
//
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#define UART_ID    uart1
#define BAUD_RATE  115200
#define TX_PIN     4
#define RX_PIN     5
#define DE_PIN     6

#define WS2812_PIN 16

static inline void put_pixel(uint32_t grb) {
    pio_sm_put_blocking(pio0, 0, grb << 8u);
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

static void set_de_tx(void) {
    gpio_put(DE_PIN, 1);
}

static void set_de_rx(void) {
    // Wait until the TX FIFO is drained and the line is idle
    uart_tx_wait_blocking(UART_ID);
    gpio_put(DE_PIN, 0);
}

int main(void) {
    // WS2812 status LED
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, WS2812_PIN, 800000, false);

    // Boot indicator: 3 blue blinks = firmware alive
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

    while (true) {
        if (uart_is_readable(UART_ID)) {
            uint8_t byte = uart_getc(UART_ID);

            put_pixel(rgb(0, 32, 0));   // green: echoing

            set_de_tx();
            uart_putc_raw(UART_ID, byte);
            set_de_rx();

            put_pixel(0);
        }
    }
}
