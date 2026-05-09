/* =============================================================================
 * TIOS — linker.ld
 * GNU ld linker script for the ARM Versatile Platform Baseboard (versatilepb).
 *
 * Purpose:
 *   Tell the linker WHERE in the address space each section of the binary
 *   should live, and how large the stack should be.
 *
 * Memory layout at runtime:
 *
 *   0x00010000  ← RAM origin (QEMU versatilepb loads kernel here)
 *               ┌─────────────────────────┐
 *               │  .text   (code)         │  ← _text_start
 *               │  .rodata (constants)    │
 *               ├─────────────────────────┤  ← _text_end
 *               │  .data   (init globals) │
 *               ├─────────────────────────┤  ← __bss_start
 *               │  .bss    (zero globals) │  zeroed by startup.s
 *               ├─────────────────────────┤  ← __bss_end
 *               │  [free heap space]      │
 *               │  ...                    │
 *               │  (stack grows down ↓)   │
 *   stack_top → └─────────────────────────┘  0x00010000 + 64K = 0x00020000
 *
 * =============================================================================
 */

/* Entry point: _start is defined in startup.s */
ENTRY(_start)

/* ---------------------------------------------------------------------------
 * MEMORY — declare the physical regions available to the linker.
 *
 * versatilepb has RAM mapped from 0x00000000, but QEMU's -kernel flag loads
 * the binary starting at 0x00010000, so we use that as our origin.
 *
 * Attributes:
 *   r = readable
 *   w = writable
 *   x = executable
 * ---------------------------------------------------------------------------
 */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x00010000, LENGTH = 64K
}

/* ---------------------------------------------------------------------------
 * SECTIONS — describe how to pack input sections into output sections
 *            and where each output section lands in RAM.
 * ---------------------------------------------------------------------------
 */
SECTIONS
{
    /* ── .text — executable code ─────────────────────────────────────────── */
    .text : {
        _text_start = .;        /* expose start address to C via extern */

        *(.text._start)         /* startup.s _start FIRST — reset vector */
        *(.text*)               /* all other .text input sections         */
        *(.rodata*)             /* read-only data (string literals, etc.) */

        _text_end = .;          /* expose end address to C via extern     */
    } > RAM

    /* ── .data — initialised global / static variables ───────────────────── */
    /* For a real system you'd load .data from Flash into RAM at boot.        */
    /* In QEMU we load the whole ELF so .data is already in place.            */
    .data : {
        *(.data*)
    } > RAM

    /* ── .bss — zero-initialised global / static variables ──────────────── */
    /* startup.s reads __bss_start and __bss_end to zero this region.         */
    .bss : {
        . = ALIGN(4);           /* align to 4-byte boundary                  */
        __bss_start = .;        /* expose to startup.s and boot.c            */

        *(.bss*)
        *(COMMON)               /* un-initialised tentative definitions       */

        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /* ── Stack ───────────────────────────────────────────────────────────── */
    /* Reserve 4 KiB for the stack.                                           */
    /* stack_top is the HIGH address; the stack grows downward toward .bss.   */
    . = ALIGN(8);               /* ABI requires 8-byte aligned stack          */
    . += 0x1000;                /* reserve 4096 bytes                         */
    stack_top = .;              /* startup.s loads sp with this address       */

    /* ── Discard debug sections we don't need in the binary ─────────────── */
    /DISCARD/ : {
        *(.ARM.exidx*)          /* C++ exception index — not used             */
        *(.comment)
    }
}
