@ ============================================================================
@ TIOS — startup.s
@ Reset vector & early boot code for ARM926EJ-S (ARMv5TE)
@
@ Responsibilities:
@   1. Set up the stack pointer
@   2. Zero the BSS segment
@   3. Copy exception vector table to 0x00000000 (required for real hardware)
@   4. Set up IRQ / FIQ mode stacks
@   5. Call kernel_main()
@   6. Hang forever if kernel_main() ever returns
@ ============================================================================

.global _start
.section .text._start   @ Use a specific section for the entry point
_start:
    b reset_handler     @ Jump over the magic number
    .ascii "TIOS"       @ Magic number for bootloader verification

reset_handler:
    @ ── Step 1: Initialise the stack pointer ─
    @ stack_top is defined at the very end of the linker script.
    @ The stack grows downward on ARM, so stack_top is the highest address
    @ of the reserved stack region.
    ldr sp, =stack_top

    @ ── Step 2: Zero the BSS segment ─
    @ Uninitialised global/static C variables live in .bss.
    @ The C standard requires them to be zero at program start.
    @ The linker script exposes __bss_start and __bss_end for us.
    ldr r0, =__bss_start    @ r0 = pointer to start of BSS
    ldr r1, =__bss_end      @ r1 = pointer to end   of BSS
    mov r2, #0              @ r2 = zero value to write

zero_bss:
    cmp r0, r1              @ have we reached the end?
    bge bss_done            @ if r0 >= r1, we are done
    str r2, [r0], #4        @ store 0 at [r0], then r0 += 4
    b   zero_bss

bss_done:

    @ ── Step 3: Copy exception vector table to 0x00000000 ─────────────────────
    @ On real ARM hardware, the CPU jumps to 0x00000000 on reset/exception by
    @ default (CP15 SCTLR.V=0).  Our vector table is linked at 0x00100000, so
    @ we must copy all 8 entries (8 × 4 = 32 bytes) plus the 8 literal pool
    @ words that immediately follow them (another 32 bytes = 64 bytes total)
    @ to low memory before enabling any interrupts.
    @
    @ VersatilePB maps writable SSRAM at 0x00000000, so this is safe.
    ldr  r0, =vector_table     @ source: linked address of vector table
    mov  r1, #0                @ destination: 0x00000000
    ldm  r0!, {r2-r9}          @ load 8 vector words (ldr pc,=... instructions)
    stm  r1!, {r2-r9}          @ store to 0x00000000–0x0000001C
    ldm  r0!, {r2-r9}          @ load 8 literal pool words that follow
    stm  r1!, {r2-r9}          @ store to 0x00000020–0x0000003C

    @ ── Step 4: Set up IRQ and FIQ mode stacks ─
    @ Defined in irq.s; also re-sets SVC sp.
    bl irq_init_stacks

    @ ── Step 4: Call boot_main() ─
    @ bl = Branch with Link — saves return address in lr register.
    @ kernel_main() is defined in kernel.c.
    bl kernel_main

    @ ── Step 5: Hang if kernel_main returns (it should not) ─
    @ An infinite loop prevents the CPU from executing garbage memory.
hang:
    b hang
