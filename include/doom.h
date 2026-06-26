/* ============================================================================
 * STAX — doom.h
 * DOOM-like games (ASCII and graphical versions)
 * ============================================================================ */

#ifndef DOOM_H
#define DOOM_H

struct window;

/* ASCII version (text-only) */
void doom_run(void);

/* Graphical version (requires framebuffer) */
void doom_engine_run(void);
void doom_engine_run_wad(const char *wadname);
void doom2_engine_run(void);
struct window *doom_create_window(void);
void doom_launch_wad(const char *wadname);
int  doom_is_running(void);
int  doom_is_loading(void);
void doom_request_quit(void);
void doom_force_cleanup(void);

#define DOOM_WIN_MARKER ((void *)0xD00D0001u)

#endif
