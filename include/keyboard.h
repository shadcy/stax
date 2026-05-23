/* ============================================================================
 * TIOS — keyboard.h
 * PL050 KMI PS/2 keyboard driver for VersatilePB
 * ============================================================================ */
#ifndef KEYBOARD_H
#define KEYBOARD_H

void kb_init(void);

/* Drain PL050 FIFO into ring buffer — call from timer ISR each tick */
void kb_poll(void);

/* Read one ASCII char from ring buffer; returns 0 if empty */
char kb_getc(void);

#endif
