/* ============================================================================
 * TIOS — irq.h
 * High-level IRQ management layer
 *
 * irq_dispatch() is called from the assembly stub in irq.s.
 * It queries the VIC, looks up the handler, and calls it.
 * ============================================================================ */

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* --------------------------------------------------------------------
 *  Assembly helpers (defined in irq.s)
 * -------------------------------------------------------------------- */
extern void irq_init_stacks(void);
extern void irq_enable(void);
extern void irq_disable(void);

/* The vector table base (for relocation if needed) */
extern uint32_t vector_table[];

/* --------------------------------------------------------------------
 *  C dispatcher — called by the assembly IRQ stub.
 *  Reads VIC status, determines the source, calls the handler.
 * -------------------------------------------------------------------- */
void irq_dispatch(void);

/* Initialise the entire IRQ subsystem:
 *  – set up mode stacks
 *  – initialise VIC
 *  – install vector table at 0x00000000 */
void irq_system_init(void);

/* Register an IRQ handler for a given VIC source */
int irq_register(uint32_t source, void (*handler)(void));

#endif /* IRQ_H */
