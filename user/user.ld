/* ============================================================================
 * STAX — User-Space Executable Linker Script
 * Linker script for standalone ARM ELF-32 binaries executing at 0x00100000
 * ============================================================================ */

ENTRY(_start)

SECTIONS
{
    . = 0x08000000;

    .text : {
        *(.text._start)
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.ARM.exidx*)
    }
}
