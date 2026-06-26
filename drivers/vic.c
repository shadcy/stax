/* ============================================================================
 * STAX — vic.c
 * Versatile Interrupt Controller (VIC) driver implementation
 * ============================================================================ */

#include "vic.h"

/* Internal slot tracking: which source is bound to each slot */
static int slot_sources[VIC_NUM_SLOTS];

/* ============================================================================
 * vic_init — reset the VIC to a known-clean state.
 * ============================================================================ */
void vic_init(void)
{
    int i;

    /* 1. Disable ALL interrupts */
    VICINTENCLEAR = 0xFFFFFFFF;

    /* 2. Clear any pending software interrupts */
    VICSOFTINTCLEAR = 0xFFFFFFFF;

    /* 3. Route everything through IRQ (not FIQ) */
    VICINTSELECT = 0;

    /* 4. Clear all vectored slots */
    for (i = 0; i < VIC_NUM_SLOTS; i++) {
        VICVECTADDRn(i) = 0;
        VICVECTCNTLn(i) = 0;
        slot_sources[i] = -1;
    }

    /* 5. Set a do-nothing default handler address.
     * In a vectored system the VIC reads VICVECTADDR when an interrupt
     * is taken; writing 0 here means we have no fallback. */
    VICDEFVECTADDR = 0;
    VICVECTADDR    = 0;
}

/* ============================================================================
 * vic_register_handler — bind a C function to a vectored slot.
 *
 *  slot    : 0 … 15
 *  source  : interrupt source number (e.g. VIC_TIMER0_INT)
 *  handler : address of the C ISR
 *
 * The VICVECTCNTLn register format:
 *   bit 5     : enable this slot
 *   bits 4:0  : interrupt source number (0–31)
 * ============================================================================ */
int vic_register_handler(int slot, uint32_t source, void (*handler)(void))
{
    if (slot < 0 || slot >= VIC_NUM_SLOTS)
        return -1;
    if (source > 31)
        return -1;

    slot_sources[slot] = (int)source;

    /* Store handler address in the vector address register */
    VICVECTADDRn(slot) = (uint32_t)handler;

    /* Enable the slot and map it to the requested source */
    VICVECTCNTLn(slot) = (1U << 5) | (source & 0x1F);

    return 0;
}
