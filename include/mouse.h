/* ============================================================================
 * TIOS — mouse.h
 * PL050 KMI1 PS/2 mouse driver interface
 * ============================================================================ */
#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
void mouse_poll(void);

extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile int mouse_buttons; /* bit0: left, bit1: right, bit2: middle */
extern volatile int mouse_changed; /* flag indicating new events */

#endif
