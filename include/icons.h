/* ============================================================================
 * STAX — icons.h
 * Industry-Standard Vector & Raster Icon Subsystem
 * ============================================================================ */

#ifndef GFX_ICONS_H
#define GFX_ICONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ICON_APP_BROWSER     = 0,
    ICON_APP_TERMINAL    = 1,
    ICON_APP_FILES       = 2,
    ICON_APP_EDITOR      = 3,
    ICON_APP_CALCULATOR  = 4,
    ICON_APP_SYSINFO     = 5,
    ICON_APP_TASKMGR     = 6,
    ICON_APP_DOOM        = 7,
    ICON_APP_SETTINGS    = 8
} app_icon_id_t;

typedef enum {
    ICON_FILE_FOLDER    = 0,
    ICON_FILE_TEXT      = 1,
    ICON_FILE_EXEC      = 2,
    ICON_FILE_IMAGE     = 3,
    ICON_FILE_AUDIO     = 4,
    ICON_FILE_FIRMWARE  = 5,
    ICON_FILE_PACKAGE   = 6,
    ICON_FILE_GENERIC   = 7
} file_icon_type_t;

/* Draw 44x44 / 40x40 Full Application Icon */
void icon_draw_app(int x, int y, app_icon_id_t id);

/* Draw Desktop Full-Size File Icon (36x40) */
void icon_draw_desktop_file(int x, int y, const char *filename, int is_dir);

/* Draw Compact File Manager Row Icon (16x16) */
void icon_draw_file_mini(int x, int y, const char *filename, int is_dir);

#ifdef __cplusplus
}
#endif

#endif /* GFX_ICONS_H */
