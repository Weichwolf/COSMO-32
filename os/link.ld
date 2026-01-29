/* COSMO-32 Linker Script */

ENTRY(_start)

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x00000000, LENGTH = 0x40000
    SRAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000
    XRAM (rwx) : ORIGIN = 0x60000000, LENGTH = 0xE0000
    FRAMEBUF (rw) : ORIGIN = 0x600E0000, LENGTH = 0x20000
}

SECTIONS
{
    /* Code in Flash */
    .text : {
        *(.text.init)
        *(.text .text.*)
    } > FLASH

    /* Read-only data */
    .rodata : {
        *(.rodata .rodata.*)
    } > FLASH

    /* Initialized data: stored in Flash, copied to SRAM */
    .data : {
        _data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        _data_end = .;
    } > SRAM AT > FLASH

    _data_load = LOADADDR(.data);

    /* Uninitialized data in SRAM */
    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        _bss_end = .;
    } > SRAM

    /* Stack at end of internal SRAM */
    _stack_top = ORIGIN(SRAM) + LENGTH(SRAM);

    /* External memory */
    .xram : {
        _extern_start = .;
        *(.xram .xram.*)
    } > XRAM

    /* Framebuffer */
    .framebuf (NOLOAD) : {
        _framebuf = .;
    } > FRAMEBUF
}
