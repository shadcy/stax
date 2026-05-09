@ ============================================================================
@ TIOS — startup.s
@ Reset vector & early boot code for ARM926EJ-S (ARMv5TE)
@
@ Responsibilities:
@   1. Set up the stack pointer
@   2. Zero the BSS segment
@   3. Set up IRQ / FIQ mode stacks
@   4. Call boot_main() in boot.c
@   5. Hang forever if boot_main() ever returns
@ ============================================================================

.global _start          @ Export _start so the linker can find the entry point
.section .text          @ Place this code in the .text section (executable)

@ ----------------------------------------------------------------------------
@ _start — reset vector / entry point
@ ----------------------------------------------------------------------------
_start:
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

    @ ── Step 3: Set up IRQ and FIQ mode stacks ─
    @ Defined in irq.s; also re-sets SVC sp.
    bl irq_init_stacks

    @ ── Step 4: Call boot_main() ─
    @ bl = Branch with Link — saves return address in lr register.
    @ boot_main() is defined in boot.c.
    bl boot_main

    @ ── Step 5: Hang if boot_main returns (it should not) ─
    @ An infinite loop prevents the CPU from executing garbage memory.
hang:
    b hang

@ ----------------------------------------------------------------------------
@ (No other code here — keep startup.s minimal.)
@ ----------------------------------------------------------------------------
