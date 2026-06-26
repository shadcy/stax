/* ============================================================================
 * STAX — app_file_manager.c
 * Windows-Explorer-style File Manager with real-time auto-refresh.
 *
 * Layout (inside client rect cx,cy,cw,ch):
 *   [0..ADDR_H)    – Address bar (current path)
 *   [ADDR_H..HDR_H] – Column header (Name | Type | Size)
 *   [HDR_H..ch)    – File list rows (ITEM_H each)
 *
 * Right-click on a file row  → per-file menu (Open, Rename, Edit)
 * Right-click on empty space → background menu (New Folder, New File)
 * Left-click on a folder     → open new FM window for that path
 * Left-click on a .TXT file  → open Nano editor
 * ============================================================================ */

#include "app_file_manager.h"
#include "wm.h"
#include "heap.h"
#include "fatfs/ff.h"
#include "framebuffer.h"
#include "string.h"
#include "font8x16.h"
#include "console.h"

/* ---- Layout constants ---- */
#define ADDR_H  20      /* address bar height           */
#define HDR_H   (ADDR_H + 18)  /* bottom of column header */
#define ITEM_H  20      /* each file row height         */

/* ---- Context menu sizes ---- */
#define CTX_FILE_W  138
#define CTX_FILE_H  90   /* 4 items */
#define CTX_BG_W    130
#define CTX_BG_H    46   /* 2 items */

#define MAX_FILES   48
#define AUTO_REFRESH_MS 2500   /* auto-reload every 2.5 s */

typedef struct {
    char     name[16];
    int      is_dir;
    uint32_t size;
} file_entry_t;

typedef struct {
    int  active;
    int  x, y;        /* client-relative top-left */
    int  file_idx;    /* >=0 → file menu; -1 → background menu */
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
    int          refresh_timer;   /* ms accumulator for auto-refresh */
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

/* Build full path: win->path + "/" + name  (root: just name) */
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

    /* "." = current dir (root after mount) — same path cmd_ls uses.
       Sub-dirs are addressed as e.g. "NEWDIR". */
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
    st->is_loaded=1;
    st->needs_reload=0;
    st->refresh_timer=0;
}

/* ======================================================= context menus === */
static void draw_ctx_file(int ax, int ay) {
    /* shadow */
    fb_fillrect(ax+3, ay+3, CTX_FILE_W, CTX_FILE_H, rgb565(60,60,60));
    /* body */
    fb_fillrect(ax, ay, CTX_FILE_W, CTX_FILE_H, rgb565(245,245,245));
    /* border */
    fb_drawline(ax, ay, ax+CTX_FILE_W, ay, rgb565(180,180,180));
    fb_drawline(ax, ay, ax, ay+CTX_FILE_H, rgb565(180,180,180));
    fb_drawline(ax+CTX_FILE_W, ay, ax+CTX_FILE_W, ay+CTX_FILE_H, rgb565(130,130,130));
    fb_drawline(ax, ay+CTX_FILE_H, ax+CTX_FILE_W, ay+CTX_FILE_H, rgb565(130,130,130));
    /* items */
    draw_text(ax+10, ay+4,  "Open / Enter", COLOR_BLACK);
    fb_fillrect(ax+4, ay+22, CTX_FILE_W-8, 1, rgb565(200,200,200));
    draw_text(ax+10, ay+26, "Rename",       COLOR_BLACK);
    fb_fillrect(ax+4, ay+44, CTX_FILE_W-8, 1, rgb565(200,200,200));
    draw_text(ax+10, ay+48, "Edit (Nano)",  COLOR_BLACK);
    fb_fillrect(ax+4, ay+66, CTX_FILE_W-8, 1, rgb565(200,200,200));
    draw_text(ax+10, ay+70, "Delete",       rgb565(220,50,50));
}

static void draw_ctx_bg(int ax, int ay) {
    fb_fillrect(ax+3, ay+3, CTX_BG_W, CTX_BG_H, rgb565(60,60,60));
    fb_fillrect(ax, ay, CTX_BG_W, CTX_BG_H, rgb565(245,245,245));
    fb_drawline(ax, ay, ax+CTX_BG_W, ay, rgb565(180,180,180));
    fb_drawline(ax, ay, ax, ay+CTX_BG_H, rgb565(180,180,180));
    fb_drawline(ax+CTX_BG_W, ay, ax+CTX_BG_W, ay+CTX_BG_H, rgb565(130,130,130));
    fb_drawline(ax, ay+CTX_BG_H, ax+CTX_BG_W, ay+CTX_BG_H, rgb565(130,130,130));
    draw_text(ax+10, ay+4,  "New Folder", COLOR_BLACK);
    fb_fillrect(ax+4, ay+22, CTX_BG_W-8, 1, rgb565(200,200,200));
    draw_text(ax+10, ay+26, "New File",   COLOR_BLACK);
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
        win->app_data=st;
    }
    if (st->needs_reload) load_files(win, st);
    if (!st->is_loaded)   load_files(win, st);

    /* ---- Background ---- */
    fb_fillrect(cx, cy, cw, ch, COLOR_WHITE);

    /* ---- Address bar ---- */
    fb_fillrect(cx, cy, cw, ADDR_H, rgb565(245,245,250));
    fb_drawline(cx, cy+ADDR_H-1, cx+cw-1, cy+ADDR_H-1, rgb565(180,180,200));
    /* folder icon */
    fb_fillrect(cx+4, cy+4, 12, 10, rgb565(255,200,0));
    fb_fillrect(cx+4, cy+2,  6,  3, rgb565(200,150,0));
    /* path text */
    const char *path_label = win->path[0] ? win->path : "C:\\  (Root)";
    draw_text(cx+20, cy+2, path_label, rgb565(30,30,100));

    /* ---- Column header ---- */
    int hy = cy + ADDR_H;
    fb_fillrect(cx, hy, cw, 18, rgb565(220,220,230));
    fb_drawline(cx, hy+17, cx+cw-1, hy+17, rgb565(160,160,180));
    draw_text(cx+22, hy+1, "Name",  rgb565(60,60,80));
    draw_text(cx + (cw*55/100), hy+1, "Type", rgb565(60,60,80));
    draw_text(cx + (cw*78/100), hy+1, "Size", rgb565(60,60,80));

    /* ---- File list ---- */
    int item_y = cy + HDR_H;
    for (int i = 0; i < st->file_count; i++) {
        if (item_y + ITEM_H > cy + ch - 2) break;

        /* Row highlight */
        if (i == st->selected_idx)
            fb_fillrect(cx, item_y, cw, ITEM_H, rgb565(205,225,255));
        else if (i & 1)
            fb_fillrect(cx, item_y, cw, ITEM_H, rgb565(248,248,255));

        /* ---- Icon ---- */
        if (st->file_list[i].is_dir) {
            /* Folder: yellow body + darker tab */
            fb_fillrect(cx+4, item_y+4,  14, 11, rgb565(255,200,0));
            fb_fillrect(cx+4, item_y+2,   7,  3, rgb565(200,150,0));
            fb_drawline(cx+4, item_y+4, cx+17, item_y+4, rgb565(180,130,0));
        } else {
            /* File: white page with folded corner */
            fb_fillrect(cx+5, item_y+2, 11, 15, COLOR_WHITE);
            fb_drawline(cx+5,  item_y+2, cx+13, item_y+2,  rgb565(150,150,200));
            fb_drawline(cx+5,  item_y+2, cx+5,  item_y+17, rgb565(150,150,200));
            fb_drawline(cx+5,  item_y+17,cx+16, item_y+17, rgb565(150,150,200));
            fb_drawline(cx+16, item_y+17,cx+16, item_y+7,  rgb565(150,150,200));
            fb_drawline(cx+16, item_y+7, cx+13, item_y+2,  rgb565(150,150,200));
            /* folded corner fill */
            fb_fillrect(cx+13, item_y+2, 4, 5, rgb565(200,200,230));
            /* .TXT indicator */
            if (is_txt(st->file_list[i].name))
                fb_fillrect(cx+7, item_y+8, 7, 2, rgb565(80,80,200));
        }

        /* ---- Name (or rename input) ---- */
        if (st->rename_active && i == st->rename_idx) {
            fb_fillrect(cx+22, item_y+2, 150, 16, COLOR_WHITE);
            fb_drawline(cx+22, item_y+2,  cx+171, item_y+2,  rgb565(0,80,200));
            fb_drawline(cx+22, item_y+18, cx+171, item_y+18, rgb565(0,80,200));
            draw_text(cx+24, item_y+2, st->rename_buf, COLOR_BLACK);
            fb_fillrect(cx+24 + st->rename_len*8, item_y+2, 2, 14, rgb565(0,100,255));
        } else {
            draw_text(cx+22, item_y+2, st->file_list[i].name,
                      st->file_list[i].is_dir ? rgb565(20,20,140) : COLOR_BLACK);
        }

        /* ---- Type ---- */
        int tx = cx + (cw*55/100);
        if (st->file_list[i].is_dir)
            draw_text(tx, item_y+2, "Folder", rgb565(80,80,140));
        else if (is_txt(st->file_list[i].name))
            draw_text(tx, item_y+2, "TXT File", rgb565(80,80,80));
        else
            draw_text(tx, item_y+2, "File", rgb565(100,100,100));

        /* ---- Size ---- */
        if (!st->file_list[i].is_dir) {
            char sz[24]; num_to_str(st->file_list[i].size, sz);
            int l=0; while(sz[l]) l++;
            sz[l]=' '; sz[l+1]='B'; sz[l+2]='\0';
            draw_text(cx + (cw*78/100), item_y+2, sz, rgb565(100,100,100));
        }

        /* Row separator */
        fb_drawline(cx, item_y+ITEM_H-1, cx+cw-1, item_y+ITEM_H-1, rgb565(235,235,245));
        item_y += ITEM_H;
    }

    /* Empty folder message */
    if (st->file_count == 0) {
        draw_text(cx+cw/2-56, cy+ch/2-8, "Folder is empty", rgb565(160,160,160));
    }

    /* Status bar at bottom */
    int sb_y = cy + ch - 14;
    fb_fillrect(cx, sb_y, cw, 14, rgb565(230,230,240));
    fb_drawline(cx, sb_y, cx+cw-1, sb_y, rgb565(180,180,200));
    char cnt_str[32];
    /* build "N items" string */
    cnt_str[0]='0'+(st->file_count/10 % 10);
    if (st->file_count >= 10) { cnt_str[1]='0'+(st->file_count%10); cnt_str[2]=' '; cnt_str[3]='i'; cnt_str[4]='t'; cnt_str[5]='e'; cnt_str[6]='m'; cnt_str[7]='s'; cnt_str[8]='\0'; }
    else { cnt_str[0]='0'+(st->file_count%10); cnt_str[1]=' '; cnt_str[2]='i'; cnt_str[3]='t'; cnt_str[4]='e'; cnt_str[5]='m'; cnt_str[6]='s'; cnt_str[7]='\0'; }
    draw_text(cx+4, sb_y, cnt_str, rgb565(80,80,80));

    /* ---- Context menus (drawn last, on top of everything) ---- */
    if (st->ctx.active) {
        int ax = cx + st->ctx.x;
        int ay = cy + st->ctx.y;
        if (st->ctx.file_idx >= 0)
            draw_ctx_file(ax, ay);
        else
            draw_ctx_bg(ax, ay);
    }
}

/* ========================================================== update ===== */
/* Called by WM every frame with elapsed ms — drives auto-refresh */
void file_manager_update(struct window *win, int dt_ms) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st) return;

    /* Do not auto-refresh while user is interacting with menus or renaming */
    if (st->ctx.active || st->rename_active) {
        st->refresh_timer = 0;
        return;
    }

    st->refresh_timer += dt_ms;
    if (st->refresh_timer >= AUTO_REFRESH_MS) {
        st->needs_reload = 1;  /* triggers reload on next draw */
        st->refresh_timer = 0;
    }
}

/* ========================================================== click ====== */
static void open_dir(struct window *win, const char *full) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    window_t *nw = wm_add_window(win->x+20, win->y+20, win->width, win->height,
                                  full, file_manager_draw_window);
    if (!nw) return;
    nw->mouse_click    = file_manager_click;
    nw->update_client  = file_manager_update;
    int k=0; while(full[k]) { nw->path[k]=full[k]; k++; } nw->path[k]='\0';
}

static void open_txt(struct window *win, const char *full) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
    extern void editor_key_event(struct window *win, char c);
    window_t *nw = wm_add_window(win->x+40, win->y+40, 500, 350,
                                  full, editor_draw_window);
    if (!nw) return;
    nw->key_event = editor_key_event;
    int k=0; while(full[k]) { nw->path[k]=full[k]; k++; } nw->path[k]='\0';
}

void file_manager_click(struct window *win, int mx, int my, int button) {
    fm_state_t *st = (fm_state_t *)win->app_data;
    if (!st || !st->is_loaded) return;

    int right = (button & 2);

    /* ---- Active context menu — dismiss or act ---- */
    if (st->ctx.active) {
        int rx = mx - st->ctx.x;
        int ry = my - st->ctx.y;

        if (st->ctx.file_idx >= 0) {
            /* File menu: Open|Rename|Edit */
            if (rx>=0 && rx<CTX_FILE_W && ry>=0 && ry<CTX_FILE_H) {
                int idx = st->ctx.file_idx;
                char full[64]; build_path(win, st->file_list[idx].name, full, 64);
                if (ry < 22) {
                    /* Open */
                    if (st->file_list[idx].is_dir) open_dir(win, full);
                    else if (is_txt(st->file_list[idx].name)) open_txt(win, full);
                } else if (ry < 44) {
                    /* Rename */
                    st->rename_active=1; st->rename_idx=idx;
                    int k=0;
                    while(st->file_list[idx].name[k] && k<14) {
                        st->rename_buf[k]=st->file_list[idx].name[k]; k++;
                    }
                    st->rename_buf[k]='\0'; st->rename_len=k;
                    win->key_event = file_manager_key_rename;
                } else if (ry < 66) {
                    /* Edit */
                    if (!st->file_list[idx].is_dir && is_txt(st->file_list[idx].name)) {
                        char fp[64]; build_path(win, st->file_list[idx].name, fp, 64);
                        open_txt(win, fp);
                    }
                } else {
                    /* Delete */
                    char fp[64]; build_path(win, st->file_list[idx].name, fp, 64);
                    f_unlink(fp[0] ? fp : st->file_list[idx].name);
                    st->needs_reload = 1;
                }
            }
        } else {
            /* Background menu: New Folder | New File */
            if (rx>=0 && rx<CTX_BG_W && ry>=0 && ry<CTX_BG_H) {
                if (ry < 22) {
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

    /* ---- Hit-test file rows ---- */
    if (my < HDR_H) { st->selected_idx=-1; return; }
    /* subtract status bar from bottom */
    if (my > (/* ch is not available here; just use large value */ 9999 - 14)) return;

    int idx = (my - HDR_H) / ITEM_H;

    if (right) {
        /* Right-click: show context menu */
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
    /* Set needs_reload on every open FM window so they all pick up changes */
    extern window_t *window_list;
    window_t *w = window_list;
    while (w) {
        if (w->draw_client == file_manager_draw_window && w->app_data)
            ((fm_state_t *)w->app_data)->needs_reload = 1;
        w = w->next;
    }
}
