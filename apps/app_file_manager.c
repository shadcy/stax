/* ============================================================================
 * STAX — app_file_manager.c
 * Authentic Ubuntu Nautilus OS File Manager with Places Sidebar & Trash Can
 * ============================================================================ */

#include "app_file_manager.h"
#include "wm.h"
#include "heap.h"
#include "fatfs/ff.h"
#include "framebuffer.h"
#include "icons.h"
#include "string.h"
#include "font.h"
#include "console.h"

/* ---- Layout constants ---- */
#define ADDR_H      32      /* Top navigation & action toolbar */
#define SIDEBAR_W   118     /* Left places sidebar width */
#define HDR_H       (ADDR_H + 20) /* Bottom of column header */
#define ITEM_H      22      /* File list row height */

/* ---- Context menu sizes ---- */
#define CTX_FILE_W  148
#define CTX_FILE_H  116     /* 5 items */
#define CTX_TRASH_H 68      /* 2 items (Restore, Delete) */
#define CTX_BG_W    136
#define CTX_BG_H    50

#define MAX_FILES   48
#define AUTO_REFRESH_MS 2500

typedef struct {
    char     name[16];
    int      is_dir;
    uint32_t size;
} file_entry_t;

typedef struct {
    int  active;
    int  x, y;
    int  file_idx;
} fm_ctx_t;

typedef struct {
    file_entry_t file_list[MAX_FILES];
    int          file_count;
    int          is_loaded;
    int          selected_idx;
    fm_ctx_t     ctx;
    int          rename_active;
    int          rename_idx;
    char         rename_buf[16];
    int          rename_len;
    int          needs_reload;
    int          refresh_timer;
    int          trash_count;
    char         prev_path[64];
} fm_state_t;

/* ============================================================= helpers === */

static void num_to_str(uint32_t n, char *buf) {
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    char tmp[12]; int i=0;
    while (n) { tmp[i++]='0'+(n%10); n/=10; }
    int j=0; while(i>0) buf[j++]=tmp[--i]; buf[j]='\0';
}

static int is_txt(const char *name) {
    int n=0; while(name[n]) n++;
    if (n<4) return 0;
    char a=name[n-3],b=name[n-2],c=name[n-1];
    return (a=='T'||a=='t') && (b=='X'||b=='x') && (c=='T'||c=='t');
}

static int is_stax(const char *name) {
    int n=0; while(name[n]) n++;
    if (n<4) return 0;
    char a=name[n-4],b=name[n-3],c=name[n-2],d=name[n-1];
    return (a=='S'||a=='s') && (b=='T'||b=='t') && (c=='A'||c=='a') && (d=='X'||d=='x');
}

static int is_in_trash(struct window *win) {
    return (strcmp(win->path, "TRASH") == 0 || strcmp(win->path, "/TRASH") == 0);
}

static void ensure_system_dirs(void) {
    f_mkdir("BIN");
    f_mkdir("DOCS");
    f_mkdir("DOWNLOADS");
    f_mkdir("TRASH");
}

/* Build full path: win->path + "/" + name (root: just name) */
static void build_path(struct window *win, const char *name, char *out, int sz) {
    int i=0, j=0;
    if (win->path[0]) {
        while (win->path[i] && j<sz-2) out[j++]=win->path[i++];
        out[j++]='/';
    }
    i=0; while (name[i] && j<sz-1) out[j++]=name[i++];
    out[j]='\0';
}

/* ================================================================ load === */
static void load_files(struct window *win, fm_state_t *st) {
    DIR dir; FILINFO fno; FRESULT res;
    st->file_count=0; st->selected_idx=-1;
    st->ctx.active=0; st->rename_active=0;

    ensure_system_dirs();

    const char *path = win->path[0] ? win->path : ".";
    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        while (st->file_count < MAX_FILES) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0]==0) break;
            if (fno.fname[0]=='.') continue;
            int i=0;
            while(i<15 && fno.fname[i]) {
                st->file_list[st->file_count].name[i]=fno.fname[i]; i++;
            }
            st->file_list[st->file_count].name[i]='\0';
            st->file_list[st->file_count].is_dir=(fno.fattrib&AM_DIR)?1:0;
            st->file_list[st->file_count].size=fno.fsize;
            st->file_count++;
        }
        f_closedir(&dir);
    }

    /* Count items in Trash */
    st->trash_count = 0;
    if (f_opendir(&dir, "TRASH") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (fno.fname[0] != '.') st->trash_count++;
        }
        f_closedir(&dir);
    }

    st->is_loaded=1;
    st->needs_reload=0;
    st->refresh_timer=0;
}

/* ========================================================== trash ops === */
static void move_to_trash(struct window *win, const char *filename) {
    ensure_system_dirs();
    char src[64], dst[64];
    build_path(win, filename, src, 64);
    strcpy(dst, "TRASH/");
    strcat(dst, filename);
    f_rename(src[0] ? src : filename, dst);
}

static void restore_from_trash(const char *filename) {
    char src[64], dst[64];
    strcpy(src, "TRASH/");
    strcat(src, filename);
    strcpy(dst, filename);
    f_rename(src, dst);
}

static void empty_trash(void) {
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "TRASH") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (fno.fname[0] != '.') {
                char p[64];
                strcpy(p, "TRASH/");
                strcat(p, fno.fname);
                f_unlink(p);
            }
        }
        f_closedir(&dir);
    }
}

static void restore_all_trash(void) {
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "TRASH") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (fno.fname[0] != '.') {
                char src[64], dst[64];
                strcpy(src, "TRASH/");
                strcat(src, fno.fname);
                strcpy(dst, fno.fname);
                f_rename(src, dst);
            }
        }
        f_closedir(&dir);
    }
}

/* ======================================================= context menus === */
static void draw_ctx_file(int ax, int ay, int in_trash) {
    int mh = in_trash ? CTX_TRASH_H : CTX_FILE_H;
    fb_fillrect(ax+3, ay+3, CTX_FILE_W, mh, rgb565(20, 22, 30));
    fb_fillrect(ax, ay, CTX_FILE_W, mh, rgb565(250, 252, 255));
    fb_drawline(ax, ay, ax+CTX_FILE_W-1, ay, theme_get_primary_accent());
    fb_drawline(ax, ay, ax, ay+mh-1, rgb565(180, 185, 200));
    fb_drawline(ax+CTX_FILE_W-1, ay, ax+CTX_FILE_W-1, ay+mh-1, rgb565(180, 185, 200));
    fb_drawline(ax, ay+mh-1, ax+CTX_FILE_W-1, ay+mh-1, rgb565(180, 185, 200));

    if (in_trash) {
        font_draw_text(ax+10, ay+5,  "Restore File", rgb565(20, 130, 60), FONT_STYLE_REGULAR);
        fb_fillrect(ax+4, ay+26, CTX_FILE_W-8, 1, rgb565(225, 228, 235));
        font_draw_text(ax+10, ay+32, "Delete Forever", rgb565(220, 50, 50), FONT_STYLE_REGULAR);
    } else {
        font_draw_text(ax+10, ay+4,  "Open / Enter", rgb565(20, 24, 35), FONT_STYLE_REGULAR);
        fb_fillrect(ax+4, ay+22, CTX_FILE_W-8, 1, rgb565(225, 228, 235));
        font_draw_text(ax+10, ay+26, "Rename",       rgb565(20, 24, 35), FONT_STYLE_REGULAR);
        fb_fillrect(ax+4, ay+44, CTX_FILE_W-8, 1, rgb565(225, 228, 235));
        font_draw_text(ax+10, ay+48, "Edit (Text)",  rgb565(20, 24, 35), FONT_STYLE_REGULAR);
        fb_fillrect(ax+4, ay+66, CTX_FILE_W-8, 1, rgb565(225, 228, 235));
        font_draw_text(ax+10, ay+70, "Move to Trash", rgb565(235, 95, 30), FONT_STYLE_REGULAR);
        fb_fillrect(ax+4, ay+88, CTX_FILE_W-8, 1, rgb565(225, 228, 235));
        font_draw_text(ax+10, ay+92, "Delete Forever", rgb565(220, 50, 50), FONT_STYLE_REGULAR);
    }
}

static void draw_ctx_bg(int ax, int ay) {
    fb_fillrect(ax+3, ay+3, CTX_BG_W, CTX_BG_H, rgb565(20, 22, 30));
    fb_fillrect(ax, ay, CTX_BG_W, CTX_BG_H, rgb565(250, 252, 255));
    fb_drawline(ax, ay, ax+CTX_BG_W-1, ay, theme_get_primary_accent());
    fb_drawline(ax, ay, ax, ay+CTX_BG_H-1, rgb565(180, 185, 200));
    fb_drawline(ax+CTX_BG_W-1, ay, ax+CTX_BG_W-1, ay+CTX_BG_H-1, rgb565(180, 185, 200));
    fb_drawline(ax, ay+CTX_BG_H-1, ax+CTX_BG_W-1, ay+CTX_BG_H-1, rgb565(180, 185, 200));

    font_draw_text(ax+10, ay+4,  "New Folder", rgb565(20, 24, 35), FONT_STYLE_REGULAR);
    fb_fillrect(ax+4, ay+24, CTX_BG_W-8, 1, rgb565(225, 228, 235));
    font_draw_text(ax+10, ay+28, "New File",   rgb565(20, 24, 35), FONT_STYLE_REGULAR);
}

/* ============================================================= draw ==== */
void file_manager_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st) {
        st = (fm_state_t *)kmalloc(sizeof(fm_state_t));
        if (!st) return;
        st->is_loaded=0; st->selected_idx=-1;
        st->ctx.active=0; st->rename_active=0;
        st->needs_reload=0; st->refresh_timer=0;
        st->trash_count=0; st->prev_path[0]='\0';
        win->app_data=st;
    }
    if (st->needs_reload) load_files(win, st);
    if (!st->is_loaded)   load_files(win, st);

    int in_trash = is_in_trash(win);

    /* Background */
    fb_fillrect(cx, cy, cw, ch, rgb565(248, 249, 252));

    /* ---- 1. Top Navigation & Action Toolbar ---- */
    fb_fillrect(cx, cy, cw, ADDR_H, rgb565(238, 241, 246));
    fb_drawline(cx, cy + ADDR_H - 1, cx + cw - 1, cy + ADDR_H - 1, rgb565(215, 218, 228));
    
    /* Back Pill */
    fb_fill_rounded_rect(cx + 6, cy + 5, 24, 22, 3, rgb565(224, 228, 238));
    font_draw_text(cx + 12, cy + 8, "<", rgb565(60, 65, 80), FONT_STYLE_REGULAR);

    /* Up Pill */
    fb_fill_rounded_rect(cx + 34, cy + 5, 24, 22, 3, rgb565(224, 228, 238));
    font_draw_text(cx + 40, cy + 8, "^", rgb565(60, 65, 80), FONT_STYLE_REGULAR);

    /* Refresh Pill */
    fb_fill_rounded_rect(cx + 62, cy + 5, 24, 22, 3, rgb565(224, 228, 238));
    font_draw_text(cx + 68, cy + 8, "R", rgb565(60, 65, 80), FONT_STYLE_REGULAR);

    /* Breadcrumbs Path Pill */
    int path_x = cx + 92;
    int act_w = in_trash ? 175 : 135;
    int path_w = cw - 92 - act_w - 8;
    if (path_w > 40) {
        fb_fill_rounded_rect(path_x, cy + 5, path_w, 22, 3, COLOR_WHITE);
        fb_drawline(path_x, cy + 5, path_x + path_w - 1, cy + 5, rgb565(205, 210, 222));
        fb_drawline(path_x, cy + 26, path_x + path_w - 1, cy + 26, rgb565(205, 210, 222));

        if (in_trash) {
            font_draw_text(path_x + 10, cy + 8, "Trash", rgb565(210, 60, 60), FONT_STYLE_REGULAR);
        } else {
            char bcrumb[64];
            if (!win->path[0]) strcpy(bcrumb, "Home (/)");
            else {
                strcpy(bcrumb, "Home  ›  ");
                strcat(bcrumb, win->path);
            }
            font_draw_text_clipped(path_x + 10, cy + 8, bcrumb, rgb565(30, 35, 50), FONT_STYLE_REGULAR,
                                   path_x + 10, cy, path_x + path_w - 4, cy + ADDR_H);
        }
    }

    /* Right Actions */
    if (in_trash) {
        /* Restore All */
        int rest_x = cx + cw - 168;
        fb_fill_rounded_rect(rest_x, cy + 5, 80, 22, 3, rgb565(40, 150, 80));
        int rw = font_get_string_width("Restore All", FONT_STYLE_REGULAR);
        font_draw_text(rest_x + (80 - rw)/2, cy + 8, "Restore All", COLOR_WHITE, FONT_STYLE_REGULAR);

        /* Empty Trash */
        int emp_x = cx + cw - 84;
        fb_fill_rounded_rect(emp_x, cy + 5, 78, 22, 3, rgb565(215, 50, 60));
        int ew = font_get_string_width("Empty Trash", FONT_STYLE_REGULAR);
        font_draw_text(emp_x + (78 - ew)/2, cy + 8, "Empty Trash", COLOR_WHITE, FONT_STYLE_REGULAR);
    } else {
        int ndir_w = 66;
        int ndir_x = cx + cw - 128;
        fb_fill_rounded_rect(ndir_x, cy + 5, ndir_w, 22, 3, theme_get_primary_accent());
        int tw = font_get_string_width("+ Folder", FONT_STYLE_REGULAR);
        font_draw_text(ndir_x + (ndir_w - tw)/2, cy + 8, "+ Folder", COLOR_WHITE, FONT_STYLE_REGULAR);

        int nfile_w = 54;
        int nfile_x = cx + cw - 58;
        fb_fill_rounded_rect(nfile_x, cy + 5, nfile_w, 22, 3, rgb565(224, 228, 238));
        int fw = font_get_string_width("+ File", FONT_STYLE_REGULAR);
        font_draw_text(nfile_x + (nfile_w - fw)/2, cy + 8, "+ File", rgb565(50, 55, 70), FONT_STYLE_REGULAR);
    }

    /* ---- 2. Left Ubuntu Places Sidebar (Clean & Simple Text) ---- */
    fb_fillrect(cx, cy + ADDR_H, SIDEBAR_W, ch - ADDR_H, rgb565(34, 36, 46));
    fb_drawline(cx + SIDEBAR_W - 1, cy + ADDR_H, cx + SIDEBAR_W - 1, cy + ch - 1, rgb565(52, 56, 70));

    font_draw_text(cx + 12, cy + ADDR_H + 6, "PLACES", rgb565(120, 125, 145), FONT_STYLE_REGULAR);

    const char *places_labels[5] = {"Home", "Documents", "Downloads", "Binaries", "Trash"};
    const char *places_paths[5]  = {"", "DOCS", "DOWNLOADS", "BIN", "TRASH"};

    int sy = cy + ADDR_H + 24;
    for (int p = 0; p < 5; p++) {
        int is_active = (p == 0 && !win->path[0]) || (p > 0 && strcmp(win->path, places_paths[p]) == 0);
        if (is_active) {
            fb_fill_rounded_rect(cx + 6, sy - 2, SIDEBAR_W - 12, 22, 3, theme_get_primary_accent());
        }

        uint16_t text_col = is_active ? COLOR_WHITE : rgb565(210, 215, 230);
        font_draw_text(cx + 14, sy + 1, places_labels[p], text_col, FONT_STYLE_REGULAR);

        if (p == 4 && st->trash_count > 0) {
            /* Trash item counter pill */
            char tcnt[8]; num_to_str(st->trash_count, tcnt);
            int badge_x = cx + SIDEBAR_W - 24;
            fb_fill_rounded_rect(badge_x, sy, 16, 14, 2, rgb565(220, 60, 60));
            font_draw_text(badge_x + 4, sy - 1, tcnt, COLOR_WHITE, FONT_STYLE_REGULAR);
        }

        sy += 24;
    }

    /* Devices Section */
    sy += 8;
    fb_drawline(cx + 6, sy, cx + SIDEBAR_W - 6, sy, rgb565(48, 52, 65));
    sy += 6;
    font_draw_text(cx + 12, sy, "DEVICES", rgb565(120, 125, 145), FONT_STYLE_REGULAR);
    sy += 18;
    font_draw_text(cx + 14, sy, "SD Card", rgb565(210, 215, 230), FONT_STYLE_REGULAR);

    /* ---- 3. Main Content Column Header ---- */
    int main_x = cx + SIDEBAR_W;
    int main_w = cw - SIDEBAR_W;
    int hy = cy + ADDR_H;

    int col_type_x = main_x + main_w - 140;
    int col_size_x = main_x + main_w - 65;
    int name_max_x = col_type_x - 8;

    fb_fillrect(main_x, hy, main_w, 20, rgb565(232, 235, 242));
    fb_drawline(main_x, hy + 19, main_x + main_w - 1, hy + 19, rgb565(205, 210, 222));

    font_draw_text(main_x + 26, hy + 2, "File Name", rgb565(90, 95, 110), FONT_STYLE_REGULAR);
    font_draw_text(col_type_x, hy + 2, "Type", rgb565(90, 95, 110), FONT_STYLE_REGULAR);
    font_draw_text(col_size_x, hy + 2, "Size", rgb565(90, 95, 110), FONT_STYLE_REGULAR);

    /* ---- 4. File Rows ---- */
    int item_y = cy + HDR_H;
    int clip_bot = cy + ch - 18;

    for (int i = 0; i < st->file_count; i++) {
        if (item_y + ITEM_H > clip_bot) break;

        if (i == st->selected_idx) {
            fb_fill_rounded_rect(main_x + 4, item_y + 1, main_w - 8, ITEM_H - 2, 3, theme_get_primary_accent());
        } else if (i & 1) {
            fb_fillrect(main_x, item_y, main_w, ITEM_H, rgb565(242, 244, 249));
        }

        uint16_t name_col = (i == st->selected_idx) ? COLOR_WHITE : 
                            (st->file_list[i].is_dir ? theme_get_primary_accent() : rgb565(20, 24, 32));
        uint16_t sub_col  = (i == st->selected_idx) ? rgb565(225, 235, 255) : rgb565(110, 115, 130);

        icon_draw_file_mini(main_x + 6, item_y + 3, st->file_list[i].name, st->file_list[i].is_dir);

        if (st->rename_active && i == st->rename_idx) {
            fb_fillrect(main_x + 26, item_y + 2, 160, 18, COLOR_WHITE);
            fb_drawline(main_x + 26, item_y + 2,  main_x + 186, item_y + 2,  theme_get_primary_accent());
            fb_drawline(main_x + 26, item_y + 19, main_x + 186, item_y + 19, theme_get_primary_accent());
            font_draw_text(main_x + 28, item_y + 3, st->rename_buf, COLOR_BLACK, FONT_STYLE_REGULAR);
        } else {
            font_draw_text_clipped(main_x + 26, item_y + 3, st->file_list[i].name, name_col, FONT_STYLE_REGULAR,
                                   main_x + 26, cy, name_max_x, cy + ch);
        }

        if (st->file_list[i].is_dir) {
            uint16_t fcol = (i == st->selected_idx) ? sub_col : theme_get_primary_accent();
            font_draw_text(col_type_x, item_y + 3, "Folder", fcol, FONT_STYLE_REGULAR);
        } else if (is_txt(st->file_list[i].name)) {
            font_draw_text(col_type_x, item_y + 3, "Text Doc", sub_col, FONT_STYLE_REGULAR);
        } else if (is_stax(st->file_list[i].name)) {
            font_draw_text(col_type_x, item_y + 3, "Firmware", sub_col, FONT_STYLE_REGULAR);
        } else {
            font_draw_text(col_type_x, item_y + 3, "Binary", sub_col, FONT_STYLE_REGULAR);
        }

        if (!st->file_list[i].is_dir) {
            char sz[24]; num_to_str(st->file_list[i].size, sz);
            int l=0; while(sz[l]) l++;
            sz[l]=' '; sz[l+1]='B'; sz[l+2]='\0';
            font_draw_text(col_size_x, item_y + 3, sz, sub_col, FONT_STYLE_REGULAR);
        }

        if (i != st->selected_idx) {
            fb_drawline(main_x + 26, item_y + ITEM_H - 1, main_x + main_w - 6, item_y + ITEM_H - 1, rgb565(232, 235, 242));
        }
        item_y += ITEM_H;
    }

    if (st->file_count == 0) {
        const char *msg = in_trash ? "Trash is empty" : "Folder is empty";
        font_draw_text(main_x + main_w/2 - 45, cy + ch/2 - 8, msg, rgb565(150, 155, 170), FONT_STYLE_REGULAR);
    }

    /* ---- 5. Bottom Status Bar ---- */
    int sb_y = cy + ch - 18;
    fb_fillrect(main_x, sb_y, main_w, 18, rgb565(234, 237, 244));
    fb_drawline(main_x, sb_y, main_x + main_w - 1, sb_y, rgb565(205, 210, 222));
    
    char cnt_str[32];
    num_to_str(st->file_count, cnt_str);
    int clen = strlen(cnt_str);
    cnt_str[clen] = ' '; cnt_str[clen+1] = 'i'; cnt_str[clen+2] = 't'; cnt_str[clen+3] = 'e'; cnt_str[clen+4] = 'm'; cnt_str[clen+5] = 's'; cnt_str[clen+6] = '\0';
    font_draw_text(main_x + 8, sb_y + 2, cnt_str, rgb565(80, 85, 100), FONT_STYLE_REGULAR);
    font_draw_text(main_x + main_w - 120, sb_y + 2, "FAT16 SD Storage", rgb565(100, 105, 120), FONT_STYLE_REGULAR);

    /* ---- 6. Context menus ---- */
    if (st->ctx.active) {
        int ax = cx + st->ctx.x;
        int ay = cy + st->ctx.y;
        if (st->ctx.file_idx >= 0)
            draw_ctx_file(ax, ay, in_trash);
        else
            draw_ctx_bg(ax, ay);
    }
}

/* ========================================================== update ===== */
void file_manager_update(struct window *win, int dt_ms) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st) return;

    if (st->ctx.active || st->rename_active) {
        st->refresh_timer = 0;
        return;
    }

    st->refresh_timer += dt_ms;
    if (st->refresh_timer >= AUTO_REFRESH_MS) {
        st->needs_reload = 1;
        st->refresh_timer = 0;
    }
}

/* ========================================================== click ====== */
static void open_dir(struct window *win, const char *full) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (st) {
        strcpy(st->prev_path, win->path);
        strcpy(win->path, full);
        st->needs_reload = 1;
    }
}

static void open_txt(struct window *win, const char *full) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
    extern void editor_key_event(struct window *win, char c);
    window_t *nw = wm_add_window(win->x+40, win->y+40, 540, 380,
                                  full, editor_draw_window);
    if (!nw) return;
    nw->key_event = editor_key_event;
    int k=0; while(full[k]) { nw->path[k]=full[k]; k++; } nw->path[k]='\0';
}

static void open_stax(struct window *win, const char *full) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    extern void fwviewer_draw_window(struct window *win, int cx, int cy, int cw, int ch);
    window_t *nw = wm_add_window(win->x+60, win->y+60, 420, 320,
                                  "Firmware Viewer", fwviewer_draw_window);
    if (!nw) return;
    int k=0; while(full[k]) { nw->path[k]=full[k]; k++; } nw->path[k]='\0';
}

void file_manager_click(struct window *win, int mx, int my, int button) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st || !st->is_loaded) return;

    int right = (button & 2);
    int in_trash = is_in_trash(win);

    /* ---- 1. Context Menu Dismiss / Action ---- */
    if (st->ctx.active) {
        int rx = mx - st->ctx.x;
        int ry = my - st->ctx.y;

        if (st->ctx.file_idx >= 0) {
            int idx = st->ctx.file_idx;
            char full[64]; build_path(win, st->file_list[idx].name, full, 64);

            if (in_trash) {
                if (rx>=0 && rx<CTX_FILE_W && ry>=0 && ry<CTX_TRASH_H) {
                    if (ry < 28) {
                        /* Restore */
                        restore_from_trash(st->file_list[idx].name);
                        st->needs_reload = 1;
                    } else {
                        /* Delete Forever */
                        char fp[64];
                        strcpy(fp, "TRASH/");
                        strcat(fp, st->file_list[idx].name);
                        f_unlink(fp);
                        st->needs_reload = 1;
                    }
                }
            } else {
                if (rx>=0 && rx<CTX_FILE_W && ry>=0 && ry<CTX_FILE_H) {
                    if (ry < 24) {
                        if (st->file_list[idx].is_dir) open_dir(win, full);
                        else if (is_txt(st->file_list[idx].name)) open_txt(win, full);
                        else if (is_stax(st->file_list[idx].name)) open_stax(win, full);
                    } else if (ry < 46) {
                        /* Rename */
                        st->rename_active=1; st->rename_idx=idx;
                        int k=0;
                        while(st->file_list[idx].name[k] && k<14) {
                            st->rename_buf[k]=st->file_list[idx].name[k]; k++;
                        }
                        st->rename_buf[k]='\0'; st->rename_len=k;
                        win->key_event = file_manager_key_rename;
                    } else if (ry < 68) {
                        /* Edit */
                        if (!st->file_list[idx].is_dir && is_txt(st->file_list[idx].name)) {
                            char fp[64]; build_path(win, st->file_list[idx].name, fp, 64);
                            open_txt(win, fp);
                        } else if (!st->file_list[idx].is_dir && is_stax(st->file_list[idx].name)) {
                            char fp[64]; build_path(win, st->file_list[idx].name, fp, 64);
                            open_stax(win, fp);
                        }
                    } else if (ry < 90) {
                        /* Move to Trash */
                        move_to_trash(win, st->file_list[idx].name);
                        st->needs_reload = 1;
                    } else {
                        /* Delete Forever */
                        char fp[64]; build_path(win, st->file_list[idx].name, fp, 64);
                        f_unlink(fp[0] ? fp : st->file_list[idx].name);
                        st->needs_reload = 1;
                    }
                }
            }
        } else {
            if (rx>=0 && rx<CTX_BG_W && ry>=0 && ry<CTX_BG_H) {
                if (ry < 25) {
                    char np[64]; build_path(win, "NEWDIR", np, 64);
                    f_mkdir(np[0] ? np : "NEWDIR");
                } else {
                    char np[64]; build_path(win, "NEWFILE.TXT", np, 64);
                    FIL f;
                    if (f_open(&f, np[0]?np:"NEWFILE.TXT",
                               FA_CREATE_NEW|FA_WRITE) == FR_OK) f_close(&f);
                }
                st->needs_reload = 1;
            }
        }
        st->ctx.active = 0;
        return;
    }

    /* ---- 2. Toolbar Actions ---- */
    if (my < ADDR_H) {
        if (mx >= 6 && mx < 30) {
            /* Back button */
            if (st->prev_path[0]) {
                strcpy(win->path, st->prev_path);
                st->prev_path[0] = '\0';
                st->needs_reload = 1;
            }
        } else if (mx >= 34 && mx < 58) {
            /* Up button */
            if (win->path[0]) {
                strcpy(st->prev_path, win->path);
                int len = strlen(win->path);
                while (len > 0 && win->path[len - 1] != '/') len--;
                if (len > 0) win->path[len - 1] = '\0';
                else win->path[0] = '\0';
                st->needs_reload = 1;
            }
        } else if (mx >= 62 && mx < 86) {
            /* Refresh */
            st->needs_reload = 1;
        } else if (in_trash) {
            if (mx >= win->width - 168 && mx < win->width - 86) {
                /* Restore All */
                restore_all_trash();
                st->needs_reload = 1;
            } else if (mx >= win->width - 84 && mx < win->width - 4) {
                /* Empty Trash */
                empty_trash();
                st->needs_reload = 1;
            }
        } else {
            if (mx >= win->width - 128 && mx < win->width - 60) {
                /* New Folder */
                char np[64]; build_path(win, "NEWDIR", np, 64);
                f_mkdir(np[0] ? np : "NEWDIR");
                st->needs_reload = 1;
            } else if (mx >= win->width - 58 && mx < win->width - 4) {
                /* New File */
                char np[64]; build_path(win, "NEWFILE.TXT", np, 64);
                FIL f;
                if (f_open(&f, np[0]?np:"NEWFILE.TXT",
                           FA_CREATE_NEW|FA_WRITE) == FR_OK) f_close(&f);
                st->needs_reload = 1;
            }
        }
        return;
    }

    /* ---- 3. Left Sidebar Places Click ---- */
    if (mx < SIDEBAR_W) {
        int sy = ADDR_H + 24;
        const char *places_paths[5] = {"", "DOCS", "DOWNLOADS", "BIN", "TRASH"};
        for (int p = 0; p < 5; p++) {
            if (my >= sy - 2 && my < sy + 22) {
                strcpy(st->prev_path, win->path);
                strcpy(win->path, places_paths[p]);
                st->needs_reload = 1;
                return;
            }
            sy += 24;
        }
        /* SD Card click */
        if (my >= sy + 20 && my < sy + 44) {
            strcpy(st->prev_path, win->path);
            win->path[0] = '\0';
            st->needs_reload = 1;
        }
        return;
    }

    /* ---- 4. Main Content Rows Click ---- */
    if (my < HDR_H) { st->selected_idx=-1; return; }

    int idx = (my - HDR_H) / ITEM_H;

    if (right) {
        st->ctx.active   = 1;
        st->ctx.x        = mx;
        st->ctx.y        = my;
        st->ctx.file_idx = (idx >= 0 && idx < st->file_count) ? idx : -1;
        if (idx >= 0 && idx < st->file_count) st->selected_idx = idx;
        return;
    }

    /* Left-click */
    if (idx < 0 || idx >= st->file_count) { st->selected_idx=-1; return; }
    st->selected_idx = idx;

    char full[64]; build_path(win, st->file_list[idx].name, full, 64);
    if (st->file_list[idx].is_dir)
        open_dir(win, full);
    else if (is_txt(st->file_list[idx].name))
        open_txt(win, full);
    else if (is_stax(st->file_list[idx].name))
        open_stax(win, full);
}

/* ================================================================ rename */
void file_manager_key_rename(struct window *win, char c) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st || !st->rename_active) return;
    int idx = st->rename_idx;
    if (idx < 0 || idx >= st->file_count) return;

    if (c == '\r' || c == '\n') {
        if (st->rename_len > 0) {
            char old_p[64], new_p[64];
            build_path(win, st->file_list[idx].name, old_p, 64);
            build_path(win, st->rename_buf,           new_p, 64);
            f_rename(old_p[0]?old_p:st->file_list[idx].name,
                     new_p[0]?new_p:st->rename_buf);
        }
        st->rename_active=0; st->needs_reload=1; win->key_event=(void*)0;
    } else if (c=='\x1b') {
        st->rename_active=0; win->key_event=(void*)0;
    } else if ((c=='\b'||c==0x7F) && st->rename_len>0) {
        st->rename_buf[--st->rename_len]='\0';
    } else if (c>=32 && c<=126 && st->rename_len<14) {
        st->rename_buf[st->rename_len++]=c;
        st->rename_buf[st->rename_len]='\0';
    }
}

/* ================================================================ refresh */
void file_manager_refresh(void) {
    extern window_t *window_list;
    window_t *w = window_list;
    while (w) {
        if (w->draw_client == file_manager_draw_window && w->app_data)
            ((fm_state_t *)w->app_data)->needs_reload = 1;
        w = w->next;
    }
}
