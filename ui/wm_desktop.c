/* ============================================================================
 * STAX — wm_desktop.c
 * Window Manager Desktop and Icons
 * ============================================================================ */

#include "wm_internal.h"
#include "wm.h"
#include "framebuffer.h"

desk_file_t desk_files[DESK_MAX];
int         desk_count    = 0;
int         desk_loaded   = 0;
int         desk_refresh  = 0;

void desk_load_files(void) {
    DIR dir; FILINFO fno;
    
    /* Mark all existing as invalid */
    for (int i = 0; i < DESK_MAX; i++) desk_files[i].valid = 0;
    
    if (f_opendir(&dir, ".") == FR_OK) {
        int idx = 0;
        while (idx < DESK_MAX) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0]==0) break;
            if (fno.fname[0]=='.') continue;
            
            /* Check if this file is already in the list to preserve x, y */
            int found = -1;
            for (int j = 0; j < desk_count; j++) {
                int k=0, match=1;
                while (fno.fname[k] || desk_files[j].name[k]) {
                    if (fno.fname[k] != desk_files[j].name[k]) { match=0; break; }
                    k++;
                }
                if (match) { found = j; break; }
            }
            
            if (found >= 0) {
                desk_files[found].valid = 1;
                desk_files[found].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
            } else {
                /* Add new */
                int slot = desk_count;
                if (slot < DESK_MAX) {
                    int k=0; while(k<15 && fno.fname[k]) { desk_files[slot].name[k]=fno.fname[k]; k++; }
                    desk_files[slot].name[k] = '\0';
                    desk_files[slot].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
                    
                    int cols = (FB_WIDTH - DESK_START_X - 10) / DESK_ICON_W;
                    if (cols < 1) cols = 1;
                    desk_files[slot].x = DESK_START_X + (slot % cols) * DESK_ICON_W;
                    desk_files[slot].y = 10 + (slot / cols) * DESK_ICON_H;
                    desk_files[slot].valid = 1;
                    desk_count++;
                }
            }
            idx++;
        }
        f_closedir(&dir);
        
        /* Compact array to remove invalid entries */
        int w = 0;
        for (int r = 0; r < desk_count; r++) {
            if (desk_files[r].valid) {
                if (w != r) desk_files[w] = desk_files[r];
                w++;
            }
        }
        desk_count = w;
    }
    desk_loaded = 1;
}
