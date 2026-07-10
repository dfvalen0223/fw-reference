#include <stdint.h>

extern "C" {
    extern uint32_t _estack;
    extern uint32_t _sdata, _edata, _sidata;
    extern uint32_t _sbss, _ebss;
    extern int main(void);
    void Reset_Handler(void);
    void Default_Handler(void) { while (1) ; }
    // FreeRTOS Handlers
    void xPortPendSVHandler(void);
    void xPortSysTickHandler(void);
    void vPortSVCHandler(void);
}

__attribute__((section(".vectors")))
void (*const vector_table[])(void) = {
    reinterpret_cast<void(*)(void)>(&_estack), // 0: Top of Stack
    Reset_Handler,                             // 1: Reset Handler
    Default_Handler,                           // 2: NMI Handler
    Default_Handler,                           // 3: Hard Fault Handler
    Default_Handler,                           // 4: MPU Fault
    Default_Handler,                           // 5: Bus Fault
    Default_Handler,                           // 6: Usage Fault
    0, 0, 0, 0,                                // 7-10: Reserved
    vPortSVCHandler,                           // 11: SVCall Handler (FreeRTOS)
    Default_Handler,                           // 12: Debug Monitor
    0,                                         // 13: Reserved
    xPortPendSVHandler,                        // 14: PendSV Handler (FreeRTOS)
    xPortSysTickHandler,                       // 15: SysTick Handler (FreeRTOS)
};

extern "C" void Reset_Handler(void) {
    for (uint32_t *src = &_sidata, *dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;
    for (uint32_t *dst = &_sbss; dst < &_ebss;)
        *dst++ = 0;
    main();
    while (1) ;
}
