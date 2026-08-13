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

    /* Call C handler in IRQ mode */
    bl      irq_dispatch

    ldr     r0, =need_schedule
    ldr     r1, [r0]
    cmp     r1, #0
    beq     no_schedule
    
    mov     r1, #0
    str     r1, [r0]
    bl      schedule

no_schedule:
    ldmfd   sp!, {r0}
    msr     spsr_cxsf, r0
    ldmfd   sp!, {r0-r3, r12, pc}^

/* ============================================================================
 * schedule — assembly context switch, called from IRQ stub in IRQ mode.
 * ============================================================================ */
    .global schedule
    .type schedule, %function
schedule:
    stmfd   sp!, {lr}               /* Save IRQ lr (return address to irq_handler_stub) */

    ldr     r0, =current_task
    ldr     r0, [r0]                /* r0 = current task TCB */

    /* Walk the circular list starting from current->next to find a READY task.
     * If we loop all the way back to current without finding one, return. */
    ldr     r1, [r0, #48]           /* r1 = candidate = current->next */

sched_scan:
    cmp     r0, r1
    beq     sched_return            /* wrapped around — no other READY task */

    ldr     r3, [r1, #52]           /* r3 = candidate->state */
    cmp     r3, #0                  /* TASK_STATE_READY? */
    beq     sched_do_switch         /* found one — switch to it */

    ldr     r1, [r1, #48]           /* candidate = candidate->next */
    b       sched_scan

sched_do_switch:
    /* Save current task r4-r11 */
    str     r4,  [r0, #0]
    str     r5,  [r0, #4]
    str     r6,  [r0, #8]
    str     r7,  [r0, #12]
    str     r8,  [r0, #16]
    str     r9,  [r0, #20]
    str     r10, [r0, #24]
    str     r11, [r0, #28]

    /* Save current task SVC sp and lr */
    mrs     r2, cpsr
    bic     r3, r2, #0x1F
    orr     r3, r3, #0x13           /* Switch to SVC mode */
    msr     cpsr_c, r3
    str     sp,  [r0, #32]
    str     lr,  [r0, #36]
    msr     cpsr_c, r2              /* Back to IRQ mode */

    /* Save caller-saved registers from IRQ stack */
    /* Stack: [sp,#4]=spsr, [sp,#8]=r0, [sp,#12]=r1, [sp,#16]=r2, [sp,#20]=r3, [sp,#24]=r12, [sp,#28]=pc */
    ldr     r3, [sp, #4]
    str     r3, [r0, #44]           /* cpsr */
    ldr     r3, [sp, #8]
    str     r3, [r0, #56]           /* r0 */
    ldr     r3, [sp, #12]
    str     r3, [r0, #60]           /* r1 */
    ldr     r3, [sp, #16]
    str     r3, [r0, #64]           /* r2 */
    ldr     r3, [sp, #20]
    str     r3, [r0, #68]           /* r3 */
    ldr     r3, [sp, #24]
    str     r3, [r0, #72]           /* r12 */
    ldr     r3, [sp, #28]
    str     r3, [r0, #40]           /* pc */

    /* Update states */
    ldr     r3, =current_task
    str     r1, [r3]
    mov     r3, #0
    str     r3, [r0, #52]
    mov     r3, #1
    str     r3, [r1, #52]

    /* Restore next task caller-saved registers to IRQ stack */
    ldr     r3, [r1, #44]
    str     r3, [sp, #4]            /* cpsr */
    ldr     r3, [r1, #56]
    str     r3, [sp, #8]            /* r0 */
    ldr     r3, [r1, #60]
    str     r3, [sp, #12]           /* r1 */
    ldr     r3, [r1, #64]
    str     r3, [sp, #16]           /* r2 */
    ldr     r3, [r1, #68]
    str     r3, [sp, #20]           /* r3 */
    ldr     r3, [r1, #72]
    str     r3, [sp, #24]           /* r12 */
    ldr     r3, [r1, #40]
    str     r3, [sp, #28]           /* pc */

    /* Restore next task SVC sp and lr */
    mrs     r2, cpsr
    bic     r3, r2, #0x1F
    orr     r3, r3, #0x13           /* Switch to SVC mode */
    msr     cpsr_c, r3
    ldr     sp,  [r1, #32]
    ldr     lr,  [r1, #36]
    msr     cpsr_c, r2              /* Back to IRQ mode */

    /* Restore next task r4-r11 */
    ldr     r4,  [r1, #0]
    ldr     r5,  [r1, #4]
    ldr     r6,  [r1, #8]
    ldr     r7,  [r1, #12]
    ldr     r8,  [r1, #16]
    ldr     r9,  [r1, #20]
    ldr     r10, [r1, #24]
    ldr     r11, [r1, #28]

sched_return:
    ldmfd   sp!, {pc}

/* ============================================================================
 * Improved exception handlers.
 * Each handler:
 *  1. Saves the faulting LR to a global variable (for GDB inspection)
 *  2. Prints a multi-character diagnostic to UART0
 *  3. Loops forever (cannot recover from these exceptions without a handler)
 *
 * To inspect after a fault in GDB:
 *   (gdb) x /1wx &stax_fault_lr
 *   (gdb) x /1wx &stax_fault_pc
 * ============================================================================ */

    .section .data
    .global stax_fault_lr
    .global stax_fault_pc
    .global stax_fault_cpsr
stax_fault_lr:   .word 0
stax_fault_pc:   .word 0
stax_fault_cpsr: .word 0

    .section .text

/* Macro: print a null-terminated string literal to UART0 */
.macro UART_PUTS str_label
    ldr r0, =0x101F1000
    ldr r1, =\str_label
1:  ldrb r2, [r1], #1
    cmp r2, #0
    beq 9f
2:  ldr r3, [r0, #0x18]
    tst r3, #0x20
    bne 2b
    str r2, [r0]
    b   1b
9:
.endm

/* Macro: print a 32-bit hex value in r2 to UART0 */
.macro UART_HEX
    ldr r0, =0x101F1000
    mov r3, #28          @ shift counter (7 nibbles * 4, start at bit 28)
10: mov r4, r2, lsr r3
    and r4, r4, #0xF
    cmp r4, #10
    addlt r4, r4, #'0'
    addge r4, r4, #('A' - 10)
11: ldr r5, [r0, #0x18]
    tst r5, #0x20
    bne 11b
    str r4, [r0]
    subs r3, r3, #4
    bge 10b
.endm

reset_handler:
    b _start

    .section .rodata
stax_msg_undef:    .asciz "\r\n[STAX FAULT] Undefined Instruction\r\n  LR="
stax_msg_svc:      .asciz "\r\n[STAX FAULT] Unexpected SVC (syscall not implemented)\r\n  LR="
stax_msg_prefetch: .asciz "\r\n[STAX FAULT] Prefetch Abort (bad instruction fetch)\r\n  LR="
stax_msg_data:     .asciz "\r\n[STAX FAULT] Data Abort (bad memory access)\r\n  LR="
stax_msg_reserved: .asciz "\r\n[STAX FAULT] Reserved Exception\r\n  LR="
stax_msg_hang:     .asciz "\r\n  System halted. Attach GDB: make gdb\r\n"

    .section .text

undef_handler:
    /* Save fault context */
    ldr r0, =stax_fault_lr
    str lr, [r0]
    mrs r1, cpsr
    ldr r0, =stax_fault_cpsr
    str r1, [r0]
    /* Print message */
    UART_PUTS stax_msg_undef
    mov r2, lr
    UART_HEX
    UART_PUTS stax_msg_hang
    b .

svc_handler:
    ldr r0, =stax_fault_lr
    str lr, [r0]
    UART_PUTS stax_msg_svc
    mov r2, lr
    UART_HEX
    UART_PUTS stax_msg_hang
    b .

prefetch_handler:
    ldr r0, =stax_fault_lr
    /* LR in abort mode points to fault + 4 */
    sub r1, lr, #4
    str r1, [r0]
    ldr r0, =stax_fault_pc
    str r1, [r0]
    UART_PUTS stax_msg_prefetch
    mov r2, r1
    UART_HEX
    UART_PUTS stax_msg_hang
    b .

data_handler:
    /* In data abort mode, LR = faulting instruction + 8 */
    sub r1, lr, #8
    ldr r0, =stax_fault_pc
    str r1, [r0]
    ldr r0, =stax_fault_lr
    str lr, [r0]
    UART_PUTS stax_msg_data
    mov r2, r1
    UART_HEX
    UART_PUTS stax_msg_hang
    b .

reserved_handler:
    ldr r0, =stax_fault_lr
    str lr, [r0]
    UART_PUTS stax_msg_reserved
    mov r2, lr
    UART_HEX
    UART_PUTS stax_msg_hang
    b .

fiq_handler:
    subs pc, lr, #4
