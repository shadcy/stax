/* ============================================================================
 * TIOS — linker.ld
 * GNU ld linker script for the ARM Versatile Platform Baseboard (versatilepb).
 * ============================================================================ */

/* Entry point: _start is defined in startup.s */
ENTRY(_start)

/* --------------------------------------------
 * MEMORY — declare the physical regions available to the linker.
 * -------------------------------------------- */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x00030000, LENGTH = 64K
}

/* --------------------------------------------
 * SECTIONS — describe how to pack input sections into output sections
 * -------------------------------------------- */
SECTIONS
{
    /* ---- Code and read-only data ---- */
    .text : {
        _text_start = .;
        *(.text._start)         /* startup.s _start FIRST */
        *(.text*)               /* all other .text input sections */
        *(.rodata*)             /* read-only data */
        _text_end = .;
    } > RAM

    /* ---- Initialised data ---- */
    .data : {
        *(.data*)
    } > RAM

    /* ---- Uninitialised data (BSS) ---- */
    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /* ---- Stack ---- */
    . = ALIGN(8);
    . += 8192;                /* 8 KB stack */
    stack_top = .;

    /* ---- Heap ---- */
    . = ALIGN(4);
    __heap_start = .;
    . += 32768;               /* 32 KB heap */
    __heap_end = .;

    /* ---- Discard debug sections ---- */
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.comment)
    }
}
