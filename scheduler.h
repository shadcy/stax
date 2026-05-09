/* ============================================================================
 * TIOS — scheduler.h
 * Minimal preemptive round-robin scheduler
 * ============================================================================ */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define MAX_TASKS       4
#define TASK_STACK_SIZE 1024

#define TASK_STATE_READY    0
#define TASK_STATE_RUNNING  1
#define TASK_STATE_BLOCKED  2

typedef struct task {
    /* Callee-saved SVC registers */
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t sp;        /* SVC stack pointer */
    uint32_t lr;        /* SVC link register */

    /* Saved interrupt context */
    uint32_t pc;        /* Interrupted program counter */
    uint32_t cpsr;      /* Interrupted CPSR (SPSR) */

    /* Task linked list */
    struct task *next;
    int state;

    /* Caller-saved registers from IRQ stack */
    uint32_t r0, r1, r2, r3;
    uint32_t r12;
} task_t;

/* Global flag set by timer ISR, cleared by IRQ stub */
extern volatile int need_schedule;

/* Currently running task */
extern task_t *current_task;

/* Initialise scheduler (creates idle task from current context) */
void scheduler_init(void);

/* Create a new task. Returns task ID or -1 on failure. */
int task_create(void (*entry)(void));

#endif /* SCHEDULER_H */
