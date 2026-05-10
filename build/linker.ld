ENTRY(_start)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x30000, LENGTH = 64K
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
    . = ALIGN(8);
    . += 8192;
    stack_top = .;
    . = ALIGN(4);
    __heap_start = .;
    . += 32768;
    __heap_end = .;
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.comment)
    }
}
