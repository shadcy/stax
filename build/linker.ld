ENTRY(_start)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x100000, LENGTH = 32M
}
SECTIONS
{
    .text : {
        _text_start = .;
        *(.text._start)
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.vfp11_veneer)
        *(.v4_bx)
        . = ALIGN(4);
        _text_end = .;
    } > RAM
    .data : {
        . = ALIGN(4);
        _data_start = .;
        *(.data*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM
    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
    . = ALIGN(8);
    . += 32768;
    stack_top = .;
    . = ALIGN(4096);
    __heap_start = .;
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.comment)
    }
}
