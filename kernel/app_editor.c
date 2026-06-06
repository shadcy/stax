/* ============================================================================
 * TIOS — app_editor.c
 * Nano-GUI Text Editor Application
 * ============================================================================ */

#include "app_editor.h"
#include "wm.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "fatfs/ff.h"
#include "heap.h"
#include "string.h"

#define MAX_EDITOR_SIZE 4096

typedef struct {
    char text[MAX_EDITOR_SIZE];
    int cursor;
    int len;
    int scroll_y;
    int is_loaded;
} editor_state_t;



void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    
    if (!st) {
        st = (editor_state_t *)kmalloc(sizeof(editor_state_t));
        if (st) {
            st->cursor = 0;
            st->len = 0;
            st->scroll_y = 0;
            st->text[0] = '\0';
            st->is_loaded = 0;
            win->app_data = st;
        } else {
            return; /* Out of memory */
        }
    }
    
    if (!st->is_loaded && win->path[0] != '\0') {
        FIL f;
        if (f_open(&f, win->path, FA_READ) == FR_OK) {
            UINT br;
            f_read(&f, st->text, MAX_EDITOR_SIZE - 1, &br);
            st->len = br;
            st->text[br] = '\0';
            st->cursor = st->len;
            f_close(&f);
        }
        st->is_loaded = 1;
    }
    
    /* Background */
    fb_fillrect(cx, cy, cw, ch, rgb565(30, 30, 40)); /* VS Code dark theme */
    
    /* Top Toolbar */
    fb_fillrect(cx, cy, cw, 24, rgb565(50, 50, 60));
    draw_text(cx + 8, cy + 4, win->path[0] ? win->path : "Untitled", COLOR_WHITE);
    draw_text(cx + cw - 130, cy + 4, "[ Ctrl+S Save ]", rgb565(150, 150, 150));
    fb_drawline(cx, cy + 24, cx + cw - 1, cy + 24, rgb565(20, 20, 30));
    
    /* Render text */
    int tx = cx + 8;
    int ty = cy + 28 - (st->scroll_y * 16);
    
    int cx_pos = tx;
    int cy_pos = ty;
    
    for (int i = 0; i <= st->len; i++) {
        if (i == st->cursor) {
            cx_pos = tx;
            cy_pos = ty;
        }
        
        if (i < st->len) {
            char c = st->text[i];
            if (c == '\n') {
                tx = cx + 8;
                ty += 16;
            } else if (c == '\r') {
                /* skip */
            } else {
                if (ty >= cy + 28 && ty + 16 < cy + ch) {
                    char str[2] = {c, '\0'};
                    draw_text(tx, ty, str, rgb565(212, 212, 212)); /* VS Code text color */
                }
                tx += 8;
                if (tx >= cx + cw - 8) {
                    tx = cx + 8;
                    ty += 16;
                }
            }
        }
    }
    
    /* Cursor block */
    if (cy_pos >= cy + 28 && cy_pos + 16 < cy + ch) {
        /* Blink logic (using global tick_count? just solid for now) */
        fb_fillrect(cx_pos, cy_pos, 8, 16, rgb565(100, 200, 255));
    }
}

void editor_key_event(struct window *win, char c) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    if (!st) return;
    
    /* Save: Ctrl+S (0x13) */
    if (c == 0x13) {
        if (win->path[0]) {
            FIL f;
            if (f_open(&f, win->path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                UINT bw;
                f_write(&f, st->text, st->len, &bw);
                f_close(&f);
            }
        }
        return;
    }
    
    /* Backspace */
    if (c == '\b' || c == 0x7F) {
        if (st->cursor > 0) {
            for (int i = st->cursor; i <= st->len; i++) {
                st->text[i - 1] = st->text[i];
            }
            st->cursor--;
            st->len--;
        }
        return;
    }
    
    /* Regular char */
    if (c >= 32 || c == '\n') {
        if (st->len < MAX_EDITOR_SIZE - 1) {
            /* insert */
            for (int i = st->len; i > st->cursor; i--) {
                st->text[i] = st->text[i - 1];
            }
            st->text[st->cursor] = c;
            st->cursor++;
            st->len++;
            st->text[st->len] = '\0';
        }
    }
}

/* Auto-save called when the window is closed via the X button */
void editor_autosave(struct window *win) {
    editor_state_t *st = (editor_state_t *)win->app_data;
    if (!st || !win->path[0] || st->len == 0) return;
    FIL f;
    if (f_open(&f, win->path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        UINT bw;
        f_write(&f, st->text, st->len, &bw);
        f_close(&f);
    }
}
