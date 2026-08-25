/* Platform-wide system control. */
#ifndef SYSTEM_H
#define SYSTEM_H

/* Performs a complete board reset. This function never returns. */
void system_reboot(void) __attribute__((noreturn));

#endif /* SYSTEM_H */
