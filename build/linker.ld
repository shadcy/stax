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
    . = ALIGN(4);
    __heap_start = .;
    . += 65536;
    __heap_end = .;
    . = ALIGN(8);
    . += 8192;
    stack_top = .;
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.comment)
    }
}
