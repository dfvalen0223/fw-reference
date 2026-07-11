MEMORY
{
    FLASH  (rx) : ORIGIN = 0x00000000, LENGTH = 512K
    SRAM   (rw) : ORIGIN = 0x10000000, LENGTH = 32K
}

SECTIONS
{
    .vectors : {
        KEEP(*(.vectors))
    } > FLASH

    .text : {
        *(.text*)
        *(.rodata*)
    } > FLASH

    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .data : ALIGN(4) {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH
    _sidata = LOADADDR(.data);

    .bss : ALIGN(4) {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);

    _end = _ebss;
    end = _ebss;
}
