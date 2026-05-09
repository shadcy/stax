/* ============================================================================
 * TIOS — irq.s
 * ARM Exception Vector Table + IRQ entry/exit stubs
 *
 * Architecture: ARM926EJ-S (ARMv5TE)
 * The vector table lives at address 0x00000000 by default.
 *
 * Vector layout (each slot = 4 bytes):
 *   0x00  Reset
 *   0x04  Undefined Instruction
 *   0x08  Software Interrupt (SVC)
 *   0x0C  Prefetch Abort
 *   0x10  Data Abort
 *   0x14  Reserved
 *   0x18  IRQ  ← timer, UART, etc.
 *   0x1C  FIQ
 *
 * Each entry is an `ldr pc, =label` pseudo-instruction which the assembler
 * expands to a PC-relative literal pool load.  This lets us branch to any
 * 32-bit address, not just nearby ones.
 * ============================================================================ */

    .section .vectors, "ax"
    .global vector_table
    .align 5                /* 32-byte alignment for cache friendliness */

vector_table:
    ldr pc, =reset_handler      /* 0x00 Reset                */
    ldr pc, =undef_handler      /* 0x04 Undefined Instruction */
    ldr pc, =svc_handler        /* 0x08 SVC / SWI            */
    ldr pc, =prefetch_handler   /* 0x0C Prefetch Abort       */
    ldr pc, =data_handler       /* 0x10 Data Abort           */
    ldr pc, =reserved_handler   /* 0x14 Reserved             */
    ldr pc, =irq_handler_stub   /* 0x18 IRQ                  */
    ldr pc, =fiq_handler        /* 0x1C FIQ                  */

/* ----------------------------------------------------------------------------
 * Stack sizes for each ARM mode.
 * We allocate separate stacks so an IRQ cannot corrupt the SVC/User stack.
 * ---------------------------------------------------------------------------- */
.equ STACK_SIZE_IRQ, 4096
.equ STACK_SIZE_FIQ, 1024

    .section .bss
    .align 4

irq_stack_base:
    .space STACK_SIZE_IRQ
irq_stack_top:

fiq_stack_base:
    .space STACK_SIZE_FIQ
fiq_stack_top:

/* ============================================================================
 * irq_init_stacks — called from C before interrupts are enabled.
 *
 * Sets up the sp (stack pointer) for IRQ, FIQ and SVC modes.
 * Must run with interrupts masked.
 * ============================================================================ */
    .section .text
    .global irq_init_stacks
    .type irq_init_stacks, %function

irq_init_stacks:
    /* Save current CPSR so we can restore mode afterwards */
    mrs r0, cpsr
    mov r1, r0                  /* keep a copy */

    /* ---- IRQ mode (IRQs disabled, FIQs enabled) ---- */
    bic r2, r1, #0x1F
    orr r2, r2, #0x12          /* IRQ mode = 0x12 */
    msr cpsr_c, r2
    ldr sp, =irq_stack_top

    /* ---- FIQ mode ---- */
    bic r2, r1, #0x1F
    orr r2, r2, #0x11          /* FIQ mode = 0x11 */
    msr cpsr_c, r2
    ldr sp, =fiq_stack_top

    /* ---- SVC mode (restore original mode) ---- */
    msr cpsr_c, r1
    ldr sp, =stack_top        /* use the stack_top defined in linker.ld */

    bx  lr

/* ============================================================================
 * irq_enable / irq_disable — thin wrappers around CPSR I-bit manipulation.
 * ============================================================================ */
    .global irq_enable
    .type irq_enable, %function
irq_enable:
    mrs r0, cpsr
    bic r0, r0, #0x80           /* clear I-bit → IRQs enabled */
    msr cpsr_c, r0
    bx  lr

    .global irq_disable
    .type irq_disable, %function
irq_disable:
    mrs r0, cpsr
    orr r0, r0, #0x80           /* set I-bit → IRQs disabled */
    msr cpsr_c, r0
    bx  lr

/* ============================================================================
 * irq_handler_stub — the actual IRQ entry point.
 *
 * 1.  Save minimum context on the IRQ stack.
 * 2.  Switch to SVC mode (so the C handler runs with the kernel stack).
 * 3.  Call irq_dispatch() in C.
 * 4.  Switch back to IRQ mode.
 * 5.  Restore context and return from interrupt (subs pc, lr, #4).
 * ============================================================================ */
    .global irq_handler_stub
    .type irq_handler_stub, %function

irq_handler_stub:
    /* --- Step 1: save context on IRQ stack --- */
    sub     lr, lr, #4          /* ARM adjusts LR_IRQ to PC+4 on IRQ entry */
    stmfd   sp!, {r0-r3, r12, lr}   /* scratch regs + return address */

    /* --- Step 2: switch to SVC mode --- */
    mrs     r0, spsr
    stmfd   sp!, {r0}           /* push SPSR onto IRQ stack temporarily */

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x13       /* SVC mode = 0x13 */
    msr     cpsr_c, r0

    /* --- Step 3: call C dispatcher --- */
    bl      irq_dispatch

    /* --- Step 4: restore IRQ mode --- */
    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x12       /* IRQ mode = 0x12 */
    msr     cpsr_c, r0

    /* --- Step 5: restore context and return --- */
    ldmfd   sp!, {r0}           /* pop SPSR */
    msr     spsr_cxsf, r0
    ldmfd   sp!, {r0-r3, r12, pc}^   /* ^ = restore CPSR from SPSR */

/* ============================================================================
 * Dummy handlers for exceptions we don't expect in normal operation.
 * They print a character over UART and spin forever so the fault is visible.
 * ============================================================================ */

    /* Reset handler — should never be reached from the vector table,
     * but we keep it here for completeness. */
reset_handler:
    b _start

    /* Undefined instruction */
undef_handler:
    ldr r0, =0x101F1000       /* UART0 base */
1:  ldr r1, [r0, #0x18]       /* UART_FR */
    tst r1, #0x20
    bne 1b
    mov r1, #'U'
    str r1, [r0]
    b .

    /* SVC / SWI */
svc_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'S'
    str r1, [r0]
    b .

    /* Prefetch abort */
prefetch_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'P'
    str r1, [r0]
    b .

    /* Data abort */
data_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'D'
    str r1, [r0]
    b .

    /* Reserved */
reserved_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'R'
    str r1, [r0]
    b .

    /* FIQ — we don't use FIQ in Phase 6a */
fiq_handler:
    subs pc, lr, #4
