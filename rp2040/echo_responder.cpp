#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#define UART_ID   uart1
#define BAUD_RATE 115200
#define TX_PIN    4
#define RX_PIN    5
#define DE_PIN    6

#define LED_PIN   PICO_DEFAULT_LED_PIN

static void set_de_tx(void) {
    gpio_put(DE_PIN, 1);
}

static void set_de_rx(void) {
    while (uart_is_writable(UART_ID)) {
        tight_loop_contents();
    }
    gpio_put(DE_PIN, 0);
}

int main(void) {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RX_PIN, GPIO_FUNC_UART);

    gpio_init(DE_PIN);
    gpio_set_dir(DE_PIN, GPIO_OUT);
    gpio_put(DE_PIN, 0);

    while (true) {
        if (uart_is_readable(UART_ID)) {
            uint8_t byte = uart_getc(UART_ID);

            gpio_put(LED_PIN, 1);

            set_de_tx();
            uart_putc_raw(UART_ID, byte);
            set_de_rx();

            gpio_put(LED_PIN, 0);
        }
    }
}
