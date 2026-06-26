#include <stddef.h>
/* ============================================================================
 * STAX — scheduler.c
 * Minimal preemptive round-robin scheduler implementation
 * ============================================================================ */

#include "scheduler.h"

static task_t task_table[MAX_TASKS];
static uint32_t task_stacks[MAX_TASKS][TASK_STACK_SIZE / 4];
static int num_tasks = 0;

volatile int need_schedule = 0;
task_t *current_task = NULL;

/* ---------------------------------------------------------------------------
 * task_exit — called if a task function ever returns.
 * --------------------------------------------------------------------------- */
void task_exit(void)
{
    current_task->state = TASK_STATE_BLOCKED;
    while (1) {
        /* On ARM926EJ-S (ARMv5), the 'wfi' instruction is undefined.
         * We just loop and wait for the scheduler to preempt us. */
    }
}

/* ============================================================================
 * scheduler_init — called from kernel_main before creating other tasks.
 * Sets up the current execution context as Task 0 (the idle task).
 * ============================================================================ */
void scheduler_init(void)
{
    int i;
    for (i = 0; i < MAX_TASKS; i++) {
        task_table[i].state = -1;  /* unused slot */
    }

    current_task = &task_table[0];
    current_task->r4 = current_task->r5 = current_task->r6 = current_task->r7 = 0;
    current_task->r8 = current_task->r9 = current_task->r10 = current_task->r11 = 0;
    current_task->r0 = current_task->r1 = current_task->r2 = current_task->r3 = 0;
    current_task->r12 = 0;
    current_task->sp = 0;   /* filled in by first context switch */
    current_task->lr = 0;   /* filled in by first context switch */
    current_task->pc = 0;   /* filled in by first context switch */
    current_task->cpsr = 0x13;  /* SVC mode, IRQs enabled */
    current_task->state = TASK_STATE_RUNNING;
    current_task->next = current_task;  /* circular list of one */
    num_tasks = 1;
}

/* ============================================================================
 * task_create — allocate a TCB and stack for a new task.
 * ============================================================================ */
int task_create(void (*entry)(void))
{
    int i;
    task_t *t;

    for (i = 1; i < MAX_TASKS; i++) {
        if (task_table[i].state == -1) break;
    }
    if (i >= MAX_TASKS) return -1;

    t = &task_table[i];
    t->r4 = t->r5 = t->r6 = t->r7 = 0;
    t->r8 = t->r9 = t->r10 = t->r11 = 0;
    t->r0 = t->r1 = t->r2 = t->r3 = 0;
    t->r12 = 0;
    t->sp = (uint32_t)&task_stacks[i][TASK_STACK_SIZE / 4];
    t->lr = (uint32_t)task_exit;
    t->pc = (uint32_t)entry;
    t->cpsr = 0x13;  /* SVC mode, IRQs enabled */
    t->state = TASK_STATE_READY;
    t->next = current_task->next;
    current_task->next = t;

    num_tasks++;
    return i;
}

int task_spawn(void (*entry)(void), uint32_t *stack_top)
{
    int i;
    task_t *t;

    for (i = 1; i < MAX_TASKS; i++) {
        if (task_table[i].state == -1) break;
    }
    if (i >= MAX_TASKS) return -1;

    t = &task_table[i];
    t->r4 = t->r5 = t->r6 = t->r7 = 0;
    t->r8 = t->r9 = t->r10 = t->r11 = 0;
    t->r0 = t->r1 = t->r2 = t->r3 = 0;
    t->r12 = 0;
    t->sp = (uint32_t)stack_top;
    t->lr = (uint32_t)task_exit;
    t->pc = (uint32_t)entry;
    t->cpsr = 0x13;
    t->state = TASK_STATE_READY;
    t->next = current_task->next;
    current_task->next = t;

    num_tasks++;
    return i;
}

void task_kill(int task_id)
{
    if (task_id < 1 || task_id >= MAX_TASKS)
        return;
    if (task_table[task_id].state == -1)
        return;

    task_t *victim = &task_table[task_id];
    victim->state = TASK_STATE_BLOCKED;

    /* Unlink victim from the circular list so the scheduler never
     * wastes a timeslice trying to switch to a dead task.            */
    task_t *prev = victim;
    while (prev->next != victim) {
        prev = prev->next;
        if (prev == victim) break;   /* safety: already unlinked */
    }
    if (prev->next == victim)
        prev->next = victim->next;
    victim->next = victim;           /* self-loop (harmless sentinel) */
}
