#ifndef ENGINE2D_H
#define ENGINE2D_H

/* ============================================================================
 * TIOS — engine2d.h
 * Mini 2D game engine abstraction.
 * ============================================================================ */

#include <stdint.h>

/* Callback structure for a game app */
typedef struct {
    void (*init)(void);
    void (*update)(int dt_ms);
    void (*draw)(void);
} EngineApp;

/* Run a game loop using the provided app structure.
   This locks to approximately 60 FPS.
   Returns when kb_is_pressed('q') or when a quit flag is set. */
void engine2d_run(EngineApp *app);

#endif
