/* ============================================================================
 * TIOS — keyboard.h
 * PL050 KMI PS/2 keyboard driver for VersatilePB
 * ============================================================================ */
#ifndef KEYBOARD_H
#define KEYBOARD_H

void kb_init(void);

/* Poll: returns ASCII char if a key was pressed, 0 if nothing ready */
char kb_getc(void);

#endif
