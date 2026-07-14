#include <cstdint>
#include <FreeRTOS.h>
#include <task.h>
#include "drivers/rs485_protocol.hpp"
#include "hal/lpc1768/rs485_lpc1768.hpp"

// LPC1768 register access
#define REG32(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

// System Control Registers
#define PCLKSEL0     REG32(0x400FC1A8) // Registro para controlar la velocidad de los periféricos

// Pin Connect Block
#define PINSEL0      REG32(0x4002C000)
#define PINSEL3      REG32(0x4002C00C) // Corregido a PINSEL3 para pines P1.16 a P1.31

// GPIO Port 1
#define GPIO1_FIODIR REG32(0x2009C020)
#define GPIO1_FIOSET REG32(0x2009C038)
#define GPIO1_FIOCLR REG32(0x2009C03C)

// UART0
#define U0_DLL       REG32(0x4000C000)
#define U0_DLM       REG32(0x4000C004)
#define U0_FCR       REG32(0x4000C008)
#define U0_LCR       REG32(0x4000C00C)
#define U0_LSR       REG32(0x4000C014)
#define U0_THR       REG32(0x4000C000)
#define U0_RBR       REG32(0x4000C000)

// PLL0 status register
#define PLL0STAT     REG32(0x400FC088)
#define PLL0_CONNECTED (1 << 25)

#define LED_PIN 18

void uart_init() {
    // PCLK left at reset default CCLK/4 = 24 MHz (PCLKSEL not used —
    // unreliable on this part). UART0 is powered at reset (PCONP bit 3).

    // Configure P0.2 as TXD0 and P0.3 as RXD0
    PINSEL0 = (PINSEL0 & ~0xF0) | 0x50;

    // 115200 baud: 24,000,000 / (16 * 115200) = 13.02 -> DL=13 (+0.16%)
    U0_LCR = 0x83; // Enable DLAB (Divisor Latch Access Bit)
    U0_DLL = 13;
    U0_DLM = 0;
    U0_LCR = 0x03; // Disables DLAB, sets 8 bits, no parity, 1 stop bit (8N1)
    U0_FCR = 0x01; // Enables FIFO
}

void uart_putchar(char c) {
    // U0_LSR Flag Full buffer - Cooperative wait: If the buffer is full, give way for a moment
    while (!(U0_LSR & (1 << 5))) {
        // Delay 1ms - It freezes the task, and frees up the CPU for another task
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    // U0_THR Output REG.
    U0_THR = c;
}

bool uart_has_data() {
    return (U0_LSR & 1);
}

char uart_raw_getchar() {
    // U0_RBR Input REG.
    return U0_RBR & 0xFF;
}

void led_task(void*) {
    // Force P1.18 as GPIO function on PINSEL3 (Bits [5:4] = 00)
    PINSEL3 &= ~(0x3 << 4);
    
    GPIO1_FIODIR |= (1 << LED_PIN);
    while (true) {
        GPIO1_FIOSET = (1 << LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
        GPIO1_FIOCLR = (1 << LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void echo_task(void*) {
    uart_init();
    const char* pll = (PLL0STAT & PLL0_CONNECTED) ? "96 MHz" : "4 MHz IRC";
    uart_putchar('[');
    for (const char* p = pll; *p; ++p) uart_putchar(*p);
    uart_putchar(']');
    uart_putchar(' ');
    const char boot_msg[] = "FW OK\r\n";
    for (const char* p = boot_msg; *p; ++p) {
        uart_putchar(*p);
    }
    while (true) {
        if (uart_has_data()) {
            char c = uart_raw_getchar();
            uart_putchar(c);
        } else {
            // If there is no data in the pins, the task blocks for 10ms
            // allowing the LED and the system to breathe.
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void rs485_task(void*) {
    hal::lpc1768::Rs485Lpc1768 hal;
    if (hal.init(115200) != hal::Status::OK) {
        while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    drivers::Rs485Protocol proto(hal);

    uint8_t seq = 0;
    while (true) {
        uint8_t payload[4];
        payload[0] = 0xAA;
        payload[1] = 0xBB;
        payload[2] = seq;
        payload[3] = static_cast<uint8_t>(seq + 1);
        proto.send_frame(payload, sizeof(payload));

        uint8_t rx_buf[128];
        std::size_t rx_len = sizeof(rx_buf);
        if (proto.recv_frame(rx_buf, rx_len)) {
            for (std::size_t i = 0; i < rx_len; ++i) {
                uart_putchar(static_cast<char>(rx_buf[i]));
            }
            uart_putchar('\n');
        }

        ++seq;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    xTaskCreate(led_task, "LED", configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(echo_task, "ECHO", configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(rs485_task, "RS485", 256, nullptr, 1, nullptr);
    
    vTaskStartScheduler();
    
    while (true) { }
}