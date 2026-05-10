ENTRY(_start)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x100000, LENGTH = 4M
}
SECTIONS
{
    .text : {
        _text_start = .;
        *(.text._start)
        *(.text*)
        *(.rodata*)
        _text_end = .;
    } > RAM
    .data : {
        *(.data*)
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
    . += 32768;
    __heap_end = .;
    . = ALIGN(8);
    . += 8192;
    stack_top = .;
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.comment)
    }
}
