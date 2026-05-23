/* ============================================================================
 * TIOS — doom.h
 * DOOM-like games (ASCII and graphical versions)
 * ============================================================================ */

#ifndef DOOM_H
#define DOOM_H

/* ASCII version (text-only) */
void doom_run(void);

/* Graphical version (requires framebuffer) */
void doom_engine_run(void);
void doom2_engine_run(void);

#endif
