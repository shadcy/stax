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
    sub     lr, lr, #4
    stmfd   sp!, {r0-r3, r12, lr}
    mrs     r0, spsr
    stmfd   sp!, {r0}
    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x13
    msr     cpsr_c, r0

    bl      irq_dispatch

    ldr     r0, =need_schedule
    ldr     r1, [r0]
    cmp     r1, #0
    beq     no_schedule
    mov     r1, #0
    str     r1, [r0]
    bl      schedule
no_schedule:

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x12
    msr     cpsr_c, r0
    ldmfd   sp!, {r0}
    msr     spsr_cxsf, r0
    ldmfd   sp!, {r0-r3, r12, pc}^

/* ============================================================================
 * schedule — assembly context switch, called from IRQ stub in SVC mode.
 * Saves current task context, restores next task context.
 * Returns to IRQ stub via r2 (preserves lr for task functions).
 * ============================================================================ */
    .global schedule
    .type schedule, %function
schedule:
    mov r2, lr              /* save return address to IRQ stub in r2 */

    ldr r0, =current_task
    ldr r0, [r0]            /* r0 = current task TCB */
    ldr r1, [r0, #48]       /* r1 = next task TCB */

    cmp r0, r1
    bxeq r2                 /* only one task, return */

    ldr r3, [r1, #52]       /* r3 = next->state */
    cmp r3, #0              /* READY? */
    bxne r2                 /* not ready, return */

    /* Save current task SVC context */
    str r4,  [r0, #0]
    str r5,  [r0, #4]
    str r6,  [r0, #8]
    str r7,  [r0, #12]
    str r8,  [r0, #16]
    str r9,  [r0, #20]
    str r10, [r0, #24]
    str r11, [r0, #28]
    str sp,  [r0, #32]
    str lr,  [r0, #36]

    /* Switch to IRQ mode to access IRQ stack */
    mrs r3, cpsr
    bic r4, r3, #0x1F
    orr r4, r4, #0x12
    msr cpsr_c, r4

    /* Save current task r0-r3, r12, pc, cpsr from IRQ stack */
    ldr r4, [sp, #4]
    ldr r5, [sp, #8]
    ldr r6, [sp, #12]
    ldr r7, [sp, #16]
    str r4, [r0, #56]
    str r5, [r0, #60]
    str r6, [r0, #64]
    str r7, [r0, #68]

    ldr r4, [sp, #20]
    ldr r5, [sp, #24]
    ldr r6, [sp, #0]
    str r4, [r0, #72]
    str r5, [r0, #40]
    str r6, [r0, #44]

    /* Restore next task r0-r3, r12, pc, cpsr to IRQ stack */
    ldr r4, [r1, #56]
    ldr r5, [r1, #60]
    ldr r6, [r1, #64]
    ldr r7, [r1, #68]
    str r4, [sp, #4]
    str r5, [sp, #8]
    str r6, [sp, #12]
    str r7, [sp, #16]

    ldr r4, [r1, #72]
    ldr r5, [r1, #40]
    ldr r6, [r1, #44]
    str r4, [sp, #20]
    str r5, [sp, #24]
    str r6, [sp, #0]

    /* Back to SVC mode */
    msr cpsr_c, r3

    /* Restore next task SVC context */
    ldr r4,  [r1, #0]
    ldr r5,  [r1, #4]
    ldr r6,  [r1, #8]
    ldr r7,  [r1, #12]
    ldr r8,  [r1, #16]
    ldr r9,  [r1, #20]
    ldr r10, [r1, #24]
    ldr r11, [r1, #28]
    ldr sp,  [r1, #32]
    ldr lr,  [r1, #36]

    /* Update current_task and states */
    ldr r3, =current_task
    str r1, [r3]
    mov r3, #0
    str r3, [r0, #52]
    mov r3, #1
    str r3, [r1, #52]

    bx r2                   /* return to IRQ stub */

/* ============================================================================
 * Dummy handlers for exceptions we do not expect in normal operation.
 * ============================================================================ */

reset_handler:
    b _start

undef_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'U'
    str r1, [r0]
    b .

svc_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'S'
    str r1, [r0]
    b .

prefetch_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'P'
    str r1, [r0]
    b .

data_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'D'
    str r1, [r0]
    b .

reserved_handler:
    ldr r0, =0x101F1000
1:  ldr r1, [r0, #0x18]
    tst r1, #0x20
    bne 1b
    mov r1, #'R'
    str r1, [r0]
    b .

fiq_handler:
    subs pc, lr, #4
    subs pc, lr, #4
