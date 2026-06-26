/* ============================================================================
 * STAX — keyboard.h
 * PL050 KMI PS/2 keyboard driver for VersatilePB
 * ============================================================================ */
#ifndef KEYBOARD_H
#define KEYBOARD_H

void kb_init(void);

/* Drain PL050 FIFO into ring buffer — call from timer ISR each tick */
void kb_poll(void);

/* Check if a specific character key is currently physically pressed down */
int kb_is_pressed(char key);

/* Read next press from ring buffer (skips releases); returns 0 if empty */
char kb_getc(void);

/* Read next event: +char = key press, -char = key release, 0 = empty */
int kb_getevent(void);

/* Discard pending events and clear held-key state */
void kb_flush(void);

/* Extended Key Constants */
#define KB_UP    0x11
#define KB_DOWN  0x12
#define KB_LEFT  0x13
#define KB_RIGHT 0x14
#define KB_SHIFT 0x15
#define KB_CTRL  0x16
#define KB_ALT   0x17

#endif
