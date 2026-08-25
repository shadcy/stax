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

#define DESK_CFG_MAGIC 0x44534B31 /* 'DSK1' */

typedef struct {
    char    name[16];
    int16_t x;
    int16_t y;
} desk_saved_pos_t;

typedef struct {
    uint32_t         magic;
    uint32_t         count;
    desk_saved_pos_t items[DESK_MAX];
} desk_cfg_t;

void desk_save_positions(void) {
    desk_cfg_t cfg;
    cfg.magic = DESK_CFG_MAGIC;
    cfg.count = 0;
    for (int i = 0; i < desk_count && cfg.count < DESK_MAX; i++) {
        if (!desk_files[i].valid) continue;
        int k = 0;
        while (k < 15 && desk_files[i].name[k]) {
            cfg.items[cfg.count].name[k] = desk_files[i].name[k];
            k++;
        }
        cfg.items[cfg.count].name[k] = '\0';
        cfg.items[cfg.count].x = (int16_t)desk_files[i].x;
        cfg.items[cfg.count].y = (int16_t)desk_files[i].y;
        cfg.count++;
    }
    FIL f;
    if (f_open(&f, "/DESKTOP.CFG", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        UINT bw = 0;
        f_write(&f, &cfg, sizeof(desk_cfg_t), &bw);
        f_close(&f);
    }
}

void desk_load_files(void) {
    DIR dir; FILINFO fno;
    
    desk_cfg_t saved;
    int has_saved = 0;
    FIL f;
    if (f_open(&f, "/DESKTOP.CFG", FA_READ) == FR_OK) {
        UINT br = 0;
        if (f_read(&f, &saved, sizeof(desk_cfg_t), &br) == FR_OK && br == sizeof(desk_cfg_t)) {
            if (saved.magic == DESK_CFG_MAGIC) {
                has_saved = 1;
            }
        }
        f_close(&f);
    }

    /* Mark all existing as invalid */
    for (int i = 0; i < DESK_MAX; i++) desk_files[i].valid = 0;
    
    if (f_opendir(&dir, ".") == FR_OK) {
        int idx = 0;
        while (idx < DESK_MAX) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            if (fno.fname[0] == '.') continue;
            if (strcmp(fno.fname, "SETTINGS.CFG") == 0 || strcmp(fno.fname, "DESKTOP.CFG") == 0) continue;
            
            /* Check if this file is already in desk_files in memory */
            int found = -1;
            for (int j = 0; j < desk_count; j++) {
                int k = 0, match = 1;
                while (fno.fname[k] || desk_files[j].name[k]) {
                    if (fno.fname[k] != desk_files[j].name[k]) { match = 0; break; }
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
                    int k = 0; while (k < 15 && fno.fname[k]) { desk_files[slot].name[k] = fno.fname[k]; k++; }
                    desk_files[slot].name[k] = '\0';
                    desk_files[slot].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
                    
                    /* Check saved positions */
                    int found_saved = -1;
                    if (has_saved) {
                        for (uint32_t s = 0; s < saved.count; s++) {
                            int sk = 0, smatch = 1;
                            while (fno.fname[sk] || saved.items[s].name[sk]) {
                                if (fno.fname[sk] != saved.items[s].name[sk]) { smatch = 0; break; }
                                sk++;
                            }
                            if (smatch) { found_saved = (int)s; break; }
                        }
                    }

                    if (found_saved >= 0) {
                        desk_files[slot].x = (int)saved.items[found_saved].x;
                        desk_files[slot].y = (int)saved.items[found_saved].y;
                    } else {
                        int rows = ((int)fb_height - TASKBAR_HEIGHT - 32) / DESK_ICON_H;
                        if (rows < 1) rows = 1;
                        desk_files[slot].x = DESK_START_X + (slot / rows) * DESK_ICON_W;
                        desk_files[slot].y = TASKBAR_HEIGHT + 14 + (slot % rows) * DESK_ICON_H;
                    }
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
