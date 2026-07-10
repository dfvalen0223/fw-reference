#include <cstdint>
#include <FreeRTOS.h>
#include <task.h>

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

#define LED_PIN 18

void uart_init() {
    // 1. Configure UART0's PCLK to use the full, undivided clock (96 MHz)
    // Clear bits 7 and 6 (~0xC0) and set bit 6 (|0x40) to write the pattern '01'
    PCLKSEL0 = (PCLKSEL0 & ~0xC0) | 0x40;

    // 2. Configure P0.2 as TXD0 and P0.3 as RXD0
    PINSEL0 = (PINSEL0 & ~0xF0) | 0x50;
    
    // 3. Baud Setting 
    U0_LCR = 0x83; // Enable DLAB (Divisor Latch Access Bit)
    U0_DLL = 52;   // Exact divisor: 96,000,000 / (16 * 115200) = 52.08 -> Rounded to 52
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

int main() {
    // We created the tasks with identical priorities
    xTaskCreate(led_task, "LED", configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
    xTaskCreate(echo_task, "ECHO", configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
    
    // The scheduler starts and control passes to the threads
    vTaskStartScheduler();
    
    //If the planner started, we'd never get here.
    while (true) { }
}